#pragma once

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"

#include <vector>

struct ExtractedObject;
class RHICommandList;

class ISceneExtractor
{
public:
	virtual ~ISceneExtractor() = default;

	virtual Result<void> PrepareRender(
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList)
	{
		(void)ctx;
		(void)frameContext;
		(void)commandList;
		return MakeOk();
	}

	virtual void Extract(std::vector<ExtractedObject>& outObjects) = 0;
};
