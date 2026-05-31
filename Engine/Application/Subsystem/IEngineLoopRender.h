#pragma once

#include "Engine/Core/Log/Result.h"

class SubsystemContext;

class IEngineLoopRender
{
public:
	virtual ~IEngineLoopRender() = default;
	virtual Result<void> RenderFrame(SubsystemContext& ctx) = 0;
};
