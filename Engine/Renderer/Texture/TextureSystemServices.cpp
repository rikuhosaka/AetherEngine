#include "Engine/Renderer/Texture/TextureSystemServices.h"

Result<std::unique_ptr<TextureSystemServices>> TextureSystemServices::Create(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator)
{
	if (device == nullptr)
	{
		return MakeFail<std::unique_ptr<TextureSystemServices>>(
			ErrorCode::InvalidArgument,
			"TextureSystemServices requires a valid RHIDevice");
	}
	return MakeOk(std::unique_ptr<TextureSystemServices>(
		new TextureSystemServices(device, descriptorAllocator)));
}

TextureSystemServices::TextureSystemServices(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator)
	: m_upload(device, descriptorAllocator)
{
}

Result<TextureHandle> TextureSystemServices::UploadTexture(
	const TextureUploadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	auto textureResult = m_upload.CreateTexture(desc, frameContext, commandList);
	if (!textureResult)
	{
		return MakeFail<TextureHandle>(textureResult.error.code, textureResult.error.message);
	}
	return MakeOk(m_pool.Add(std::move(textureResult.value)));
}
