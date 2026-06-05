#pragma once

#include "Engine/Application/Subsystem/SubsystemTypes.h"

#include <Windows.h>

#include <cstdint>
#include <filesystem>

struct EngineLoopConfig
{
	HINSTANCE hInstance = nullptr;
	std::filesystem::path shaderRoot{};
	std::filesystem::path compiledShaderRoot{};
	std::filesystem::path dx12DebugConfigPath{};

	uint32_t initialWidth = 1280;
	uint32_t initialHeight = 720;

	float maxDeltaSeconds = EngineConstants::kMaxDeltaSeconds;
	bool vsync = true;
};
