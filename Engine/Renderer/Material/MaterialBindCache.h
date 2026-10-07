#pragma once

#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Material/MaterialTypes.h"

class RHIDevice;
class RHICommandList;
class RHITexture;
class TextureSystemServices;

class MaterialBindCache
{
public:
	explicit MaterialBindCache(RHIDevice* device);

	[[nodiscard]] bool Bind(
		FrameContext& frameContext,
		RHICommandList* commandList,
		const Material& material,
		const MaterialInstance& instance,
		TextureSystemServices& textureServices,
		RHITexture* shadowMap) const;

private:
	RHIDevice* m_device = nullptr;
};
