#include "Runtime/Renderer/Texture/TextureSystemServices.h"

#include "Runtime/Core/Log/LogMacros.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/RHI/Interface/RHICommandList.h"

Result<std::unique_ptr<TextureSystemServices>> TextureSystemServices::Create(
	RHIDevice* device,
	RHIDescriptorAllocator* descriptorAllocator)
{
	if (device == nullptr)
	{
		return FailInternal<std::unique_ptr<TextureSystemServices>>(
			LogCategory::Renderer,
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
	RHICommandList* commandList,
	std::string_view cacheKey)
{
	if (!cacheKey.empty())
	{
		if (const TextureHandle cached = m_loadCache.Find(cacheKey); cached.IsValid())
		{
			return MakeOk(cached);
		}
	}

	auto textureResult = m_upload.CreateTexture(desc, frameContext, commandList);
	if (!textureResult)
	{
		return MakeFail<TextureHandle>(textureResult.error.code, textureResult.error.message);
	}

	const TextureHandle handle = m_pool.Add(std::move(textureResult.value));
	m_loadCache.Store(cacheKey, handle);
	return MakeOk(handle);
}
