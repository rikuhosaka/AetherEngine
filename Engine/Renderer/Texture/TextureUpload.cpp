#include "Engine/Renderer/Texture/TextureUpload.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Common/RHIResource.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIDescriptorAllocator.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"
#include "Engine/RHI/Interface/RHITexture.h"

#include <cstring>
#include <vector>

namespace
{
constexpr size_t kTextureDataPitchAlignment = 256;
constexpr size_t kTextureDataPlacementAlignment = 512;

uint32_t BytesPerPixel(ERHIFormat format)
{
	switch (format)
	{
	case ERHIFormat::R8G8B8A8_UNORM:
	case ERHIFormat::R8G8B8A8_UNORM_SRGB:
	case ERHIFormat::B8G8R8A8_UNORM:
		return 4;
	default:
		return 4;
	}
}

[[nodiscard]] size_t AlignUp(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

[[nodiscard]] uint32_t GetMipDimension(uint32_t dimension, uint32_t mipSlice)
{
	const uint32_t shifted = dimension >> mipSlice;
	return shifted > 0 ? shifted : 1u;
}

struct MipUploadLayout
{
	size_t offset = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t rowPitch = 0;
	uint32_t sourceRowPitch = 0;
	size_t size = 0;
};

[[nodiscard]] uint32_t ResolveMipCount(const TextureUploadDesc& desc)
{
	if (!desc.mips.empty() && desc.mipLevels == 0)
	{
		return static_cast<uint32_t>(desc.mips.size());
	}

	return desc.mipLevels;
}

[[nodiscard]] Result<std::vector<MipUploadLayout>> BuildMipUploadLayouts(
	const TextureUploadDesc& desc,
	uint32_t mipCount)
{
	const uint32_t bytesPerPixel = BytesPerPixel(desc.format);
	std::vector<MipUploadLayout> layouts;
	layouts.reserve(mipCount);

	size_t currentOffset = 0;
	for (uint32_t mipIndex = 0; mipIndex < mipCount; ++mipIndex)
	{
		const TextureMipData& mip = desc.mips[mipIndex];

		MipUploadLayout layout{};
		layout.width = mip.width != 0
			? mip.width
			: GetMipDimension(desc.width, mipIndex);
		layout.height = mip.height != 0
			? mip.height
			: GetMipDimension(desc.height, mipIndex);
		if (layout.width == 0 || layout.height == 0)
		{
			return FailRuntime<std::vector<MipUploadLayout>>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Texture upload mip dimensions are invalid");
		}

		layout.sourceRowPitch = mip.rowPitch != 0
			? mip.rowPitch
			: layout.width * bytesPerPixel;
		layout.rowPitch = static_cast<uint32_t>(
			AlignUp(layout.width * bytesPerPixel, kTextureDataPitchAlignment));
		layout.size = static_cast<size_t>(layout.rowPitch) * layout.height;

		const size_t requiredSourceBytes =
			static_cast<size_t>(layout.sourceRowPitch) * (layout.height - 1)
			+ layout.width * bytesPerPixel;
		if (mip.pixels.size() < requiredSourceBytes)
		{
			return FailRuntime<std::vector<MipUploadLayout>>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Texture upload mip pixel data is too small");
		}

		currentOffset = AlignUp(currentOffset, kTextureDataPlacementAlignment);
		layout.offset = currentOffset;
		layouts.push_back(layout);
		currentOffset += layout.size;
	}

	return MakeOk(std::move(layouts));
}

void WriteMipToUploadBuffer(
	std::byte* uploadBase,
	const TextureMipData& mip,
	const MipUploadLayout& layout,
	uint32_t bytesPerPixel)
{
	const uint32_t copyBytesPerRow = layout.width * bytesPerPixel;

	if (layout.rowPitch == layout.sourceRowPitch && layout.size == mip.pixels.size())
	{
		std::memcpy(uploadBase + layout.offset, mip.pixels.data(), layout.size);
		return;
	}

	for (uint32_t row = 0; row < layout.height; ++row)
	{
		const size_t srcOffset = static_cast<size_t>(layout.sourceRowPitch) * row;
		const size_t dstOffset = layout.offset + static_cast<size_t>(layout.rowPitch) * row;
		std::memcpy(
			uploadBase + dstOffset,
			mip.pixels.data() + srcOffset,
			copyBytesPerRow);
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

	const uint32_t mipCount = ResolveMipCount(desc);
	if (mipCount == 0 || mipCount != desc.mips.size())
	{
		return FailRuntime<std::unique_ptr<Texture>>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Texture upload mip count does not match mip data");
	}

	auto layoutsResult = BuildMipUploadLayouts(desc, mipCount);
	if (!layoutsResult)
	{
		return MakeFail<std::unique_ptr<Texture>>(
			layoutsResult.error.code,
			layoutsResult.error.message);
	}

	const std::vector<MipUploadLayout>& layouts = layoutsResult.value;
	const size_t totalUploadSize = layouts.back().offset + layouts.back().size;

	RHIUploadAllocation allocation = frameContext.uploadBuffer->Allocate(
		totalUploadSize,
		kTextureDataPlacementAlignment);
	if (allocation.cpuAddress == nullptr)
	{
		return FailRuntime<std::unique_ptr<Texture>>(
			LogCategory::Renderer,
			ErrorCode::OutOfMemory,
			"Upload buffer allocation failed for texture data");
	}

	const uint32_t bytesPerPixel = BytesPerPixel(desc.format);
	auto* uploadBase = static_cast<std::byte*>(allocation.cpuAddress);
	for (uint32_t mipIndex = 0; mipIndex < mipCount; ++mipIndex)
	{
		WriteMipToUploadBuffer(
			uploadBase,
			desc.mips[mipIndex],
			layouts[mipIndex],
			bytesPerPixel);
	}

	RHITextureDesc textureDesc{};
	textureDesc.Width = desc.width;
	textureDesc.Height = desc.height;
	textureDesc.MipLevels = mipCount;
	textureDesc.Usage = desc.usage;
	textureDesc.Format = desc.format;
	textureDesc.DebugName = desc.DebugName;

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

	for (uint32_t mipIndex = 0; mipIndex < mipCount; ++mipIndex)
	{
		const MipUploadLayout& layout = layouts[mipIndex];
		commandList->CopyTextureRegion(
			texture->resource.get(),
			mipIndex,
			frameContext.uploadBuffer,
			allocation.offset + layout.offset,
			layout.rowPitch,
			layout.height);
	}

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
