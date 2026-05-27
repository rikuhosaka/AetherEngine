#pragma once

#include "Engine/Renderer/Frame/FrameContext.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

class RHIDevice;
class RHIDescriptorAllocator;

class TextureUpload
{
public:
	TextureUpload(RHIDevice* device, RHIDescriptorAllocator* descriptorAllocator);

	[[nodiscard]] std::unique_ptr<Texture> CreateTexture(
		const TextureUploadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList) const;

private:
	RHIDevice* m_device = nullptr;
	RHIDescriptorAllocator* m_descriptorAllocator = nullptr;
};
