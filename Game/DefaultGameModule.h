#pragma once

#include "Game/IGameModule.h"
#include "Game/QuadSceneAssets.h"

class DefaultGameModule final : public IGameModule
{
public:
	Result<void> OnInit(IGameHost& host) override;
	Result<void> OnPrepareRender(
		IGameHost& host,
		FrameContext& frameContext,
		RHICommandList* commandList) override;
	void OnTick(IGameHost& host, float deltaSeconds) override;
	void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) override;
	void OnShutdown(IGameHost& host) override;

private:
	QuadSceneAssets m_quad{};
};
