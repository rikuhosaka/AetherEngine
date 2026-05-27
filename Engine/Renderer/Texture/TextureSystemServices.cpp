#include "Engine/Renderer/Texture/TextureSystemServices.h"

std::unique_ptr<TextureSystemServices> TextureSystemServices::Create(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator)
{
	if (device == nullptr)
	{
		return nullptr;
	}
	return std::unique_ptr<TextureSystemServices>(
		new TextureSystemServices(device, descriptorAllocator));
}

TextureSystemServices::TextureSystemServices(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator)
	: m_upload(device, descriptorAllocator)
{
}

TextureHandle TextureSystemServices::UploadTexture(
	const TextureUploadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	std::unique_ptr<Texture> texture = m_upload.CreateTexture(desc, frameContext, commandList);
	if (texture == nullptr)
	{
		return {};
	}
	return m_pool.Add(std::move(texture));
}
