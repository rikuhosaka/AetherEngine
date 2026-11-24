#pragma once

#include "Runtime/Renderer/Core/RendererConfig.h"

#include <filesystem>

// RenderSystem の初期化に必要な値。エンジンの設定からは EngineRender が写す。
struct RenderSystemConfig
{
	RendererConfig renderer{};
	std::filesystem::path dx12DebugConfigPath{};
	bool vsync = true;
};
