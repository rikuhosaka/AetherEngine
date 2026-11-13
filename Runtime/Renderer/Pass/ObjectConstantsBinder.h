#pragma once

#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/Material/MaterialTypes.h"
#include "Runtime/Renderer/Scene/RenderViewTypes.h"

class RHIDevice;
class RHICommandList;

class ObjectConstantsBinder
{
public:
	explicit ObjectConstantsBinder(RHIDevice* device);

	[[nodiscard]] bool Bind(
		FrameContext& frameContext,
		RHICommandList* commandList,
		const Material& material,
		const ObjectConstants& constants) const;

private:
	RHIDevice* m_device = nullptr;
};
