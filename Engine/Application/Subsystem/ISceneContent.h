#pragma once

#include "Engine/Core/Log/Result.h"

class RHICommandList;
class SubsystemContext;
class World;
struct FrameContext;

class ISceneContent
{
public:
	virtual ~ISceneContent() = default;

	virtual Result<void> Load(
		World& world,
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList) = 0;
};
