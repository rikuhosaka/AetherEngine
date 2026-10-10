#pragma once

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"

#include <vector>

struct ExtractedLighting;
struct ExtractedObject;
struct ExtractedView;
class RHICommandList;

class ISceneExtractor
{
public:
	virtual ~ISceneExtractor() = default;

	virtual Result<void> LoadContent(
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList)
	{
		(void)ctx;
		(void)frameContext;
		(void)commandList;
		return MakeOk();
	}

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

	virtual void ExtractView(ExtractedView& outView)
	{
		(void)outView;
	}

	virtual void ExtractLighting(ExtractedLighting& outLighting)
	{
		(void)outLighting;
	}
};
