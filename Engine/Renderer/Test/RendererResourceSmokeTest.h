#pragma once

#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Frame/FrameContext.h"

#include <filesystem>
#include <string>

struct RendererResourceSmokeResult
{
	bool success = false;
	RenderItem renderItem{};
	std::string error{};
};

[[nodiscard]] RendererResourceSmokeResult BuildRendererResourceSmokeScene(
	Renderer& renderer,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot);
