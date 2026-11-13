#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/Texture/TextureTypes.h"

#include <memory>

class RHIDevice;
class RHIDescriptorAllocator;

class TextureUpload
{
public:
	TextureUpload(RHIDevice* device, RHIDescriptorAllocator* descriptorAllocator);

	[[nodiscard]] Result<std::unique_ptr<Texture>> CreateTexture(
		const TextureUploadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList) const;

private:
	RHIDevice* m_device = nullptr;
	RHIDescriptorAllocator* m_descriptorAllocator = nullptr;
};
