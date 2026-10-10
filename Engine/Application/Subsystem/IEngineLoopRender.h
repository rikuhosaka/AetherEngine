#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

#include <span>

class RHICommandList;
class SubsystemContext;

class IEngineLoopRender
{
public:
	virtual ~IEngineLoopRender() = default;

	virtual Result<void> DrawFrame(
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList,
		const ExtractedView& view,
		const ExtractedLighting& lighting,
		std::span<const ExtractedObject> objects) = 0;
};
