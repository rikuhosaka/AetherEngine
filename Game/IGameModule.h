#pragma once

#include "Engine/Core/Log/Result.h"

#include <vector>

struct ExtractedObject;

class IGameHost;

class IGameModule
{
public:
	virtual ~IGameModule() = default;

	virtual Result<void> OnInit(IGameHost& host) = 0;
	virtual void OnTick(IGameHost& host, float deltaSeconds) = 0;
	virtual void OnExtract(IGameHost& host, std::vector<ExtractedObject>& outObjects) = 0;
	virtual void OnShutdown(IGameHost& host) = 0;
};

IGameModule* CreateGameModule();
