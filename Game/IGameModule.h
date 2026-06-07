#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Frame/FrameContext.h"

#include <vector>

struct ExtractedLighting;
struct ExtractedObject;
struct ExtractedView;

class IGameHost;
class RHICommandList;

class IGameModule
{
public:
	virtual ~IGameModule() = default;

	virtual Result<void> OnInit(IGameHost& host) = 0;
	virtual Result<void> OnPrepareRender(
		IGameHost& host,
		FrameContext& frameContext,
		RHICommandList* commandList)
	{
		(void)host;
		(void)frameContext;
		(void)commandList;
		return MakeOk();
	}
	virtual void OnTick(IGameHost& host, float deltaSeconds) = 0;
	virtual void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) = 0;
	virtual void OnExtractView(IGameHost& host, ExtractedView& outView)
	{
		(void)host;
		(void)outView;
	}
	virtual void OnExtractLighting(IGameHost& host, ExtractedLighting& outLighting)
	{
		(void)host;
		(void)outLighting;
	}
	virtual void OnShutdown(IGameHost& host) = 0;
};

IGameModule* CreateGameModule();
