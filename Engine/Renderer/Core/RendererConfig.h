#pragma once

#include "Engine/Renderer/Core/ShaderSourcePolicy.h"

#include <filesystem>

struct RendererConfig
{
	std::filesystem::path shaderRoot{};
	std::filesystem::path compiledShaderRoot{};
	ShaderSourcePolicy shaderSourcePolicy = ShaderSourcePolicy::PreferPrecompiled;
};
