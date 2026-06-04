#pragma once

#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Frame/FrameContext.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

#include <filesystem>
#include <string>

struct RendererResourceSmokeSceneConstants
{
	float mvp[16]{};
};

struct RendererResourceSmokeMaterialConstants
{
	float tint[4]{ 0.26f, 0.52f, 0.96f, 1.0f };
};

struct RendererResourceSmokeResult
{
	bool success = false;
	ExtractedObject extractedObject{};
	RendererResourceSmokeSceneConstants sceneConstants{};
	RendererResourceSmokeMaterialConstants materialConstants{};
	std::string error{};
};

[[nodiscard]] RendererResourceSmokeResult BuildRendererResourceSmokeScene(
	Renderer& renderer,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot);
