#pragma once

#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Scene/RenderViewTypes.h"

class RHIDevice;
class RHICommandList;

class FrameConstantsBinder
{
public:
	explicit FrameConstantsBinder(RHIDevice* device);

	[[nodiscard]] bool Bind(
		FrameContext& frameContext,
		RHICommandList* commandList,
		const Material& material,
		const FrameConstants& constants) const;

private:
	RHIDevice* m_device = nullptr;
};
