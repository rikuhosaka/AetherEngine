#include "Engine/Renderer/Texture/TextureUpload.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Common/RHIResource.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"
#include "Engine/RHI/Interface/RHITexture.h"

namespace
{
uint32_t BytesPerPixel(ERHIFormat format)
{
	switch (format)
	{
	case ERHIFormat::R8G8B8A8_UNORM:
	case ERHIFormat::B8G8R8A8_UNORM:
		return 4;
	default:
		return 4;
	}
}
} // namespace

TextureUpload::TextureUpload(RHIDevice* device, RHIDescriptorAllocator* descriptorAllocator)
	: m_device(device)
	, m_descriptorAllocator(descriptorAllocator)
{
}

Result<std::unique_ptr<Texture>> TextureUpload::CreateTexture(
	const TextureUploadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList) const
{
	if (m_device == nullptr || frameContext.uploadBuffer == nullptr || commandList == nullptr)
	{
		return FailInternal<std::unique_ptr<Texture>>(
			LogCategory::Renderer,
			ErrorCode::InvalidArgument,
			"Texture upload requires a valid device, upload buffer, and command list");
	}

	if (desc.mips.empty() || desc.width == 0 || desc.height == 0)
	{
		return FailRuntime<std::unique_ptr<Texture>>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture upload requires valid dimensions and mip data");
	}

	const TextureMipData& mip0 = desc.mips.front();
	const uint32_t rowPitch = mip0.rowPitch != 0
		? mip0.rowPitch
		: desc.width * BytesPerPixel(desc.format);
	const size_t uploadSize = static_cast<size_t>(rowPitch) * desc.height;

	RHIUploadAllocation allocation = frameContext.uploadBuffer->Allocate(uploadSize, 256);
	if (allocation.cpuAddress == nullptr)
	{
		return FailRuntime<std::unique_ptr<Texture>>(
			LogCategory::Renderer,
			ErrorCode::OutOfMemory,
			"Upload buffer allocation failed for texture data");
	}

	std::memcpy(allocation.cpuAddress, mip0.pixels.data(), mip0.pixels.size());

	RHITextureDesc textureDesc{};
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.MipLevels = desc.mipLevels;
	textureDesc.Usage = desc.usage;
	textureDesc.Format = desc.format;

	auto texture = std::make_unique<Texture>();
	texture->desc = textureDesc;
	auto textureResult = m_device->CreateTexture(textureDesc);
	if (!textureResult)
	{
		return MakeFail<std::unique_ptr<Texture>>(
			textureResult.error.code,
			textureResult.error.message);
	}
	texture->resource = std::move(textureResult.value);

	commandList->CopyTextureRegion(
		texture->resource.get(),
		0,
		frameContext.uploadBuffer,
		allocation.offset,
		rowPitch,
		desc.height);
	commandList->ResourceBarrier(texture->resource.get(), ERHIResourceState::PixelShaderResource);

	if (m_descriptorAllocator != nullptr)
	{
		auto srvResult = m_device->CreateShaderResourceView(texture->resource.get(), m_descriptorAllocator);
		if (srvResult)
		{
			texture->srv = srvResult.value;
			texture->srvValid = texture->srv.gpu.ptr != 0;
		}
		else
		{
			LogResult(srvResult, LogCategory::Renderer);
		}
	}

	return MakeOk(std::move(texture));
}
