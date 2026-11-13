#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Core/Renderer.h"
#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/Scene/RenderLightingTypes.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"
#include "Runtime/Renderer/Scene/RenderViewTypes.h"

#include <filesystem>
#include <string>

struct RendererResourceSmokeMaterialConstants
{
	float tint[4]{ 0.26f, 0.52f, 0.96f, 1.0f };
};

struct RendererResourceSmokeResult
{
	bool success = false;
	ExtractedView view{};
	ExtractedLighting lighting{};
	FrameConstants frameConstants{};
	ExtractedObject extractedObject{};
	RendererResourceSmokeMaterialConstants materialConstants{};
	std::string error{};
};

[[nodiscard]] Result<void> RunRendererResourceSmokeLayoutTests();

[[nodiscard]] RendererResourceSmokeResult BuildRendererResourceSmokeScene(
	Renderer& renderer,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot);
