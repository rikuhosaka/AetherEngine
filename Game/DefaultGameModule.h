#pragma once

#include "Game/IGameModule.h"

class DefaultGameModule final : public IGameModule
{
public:
	Result<void> OnInit(IGameHost& host) override;
	void OnTick(IGameHost& host, float deltaSeconds) override;
	void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) override;
	void OnShutdown(IGameHost& host) override;
};
