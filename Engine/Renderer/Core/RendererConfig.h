#pragma once

#include "Engine/Renderer/Core/ShaderSourcePolicy.h"

#include <filesystem>

struct RendererConfig
{
	std::filesystem::path shaderRoot{};
	std::filesystem::path compiledShaderRoot{};
	std::filesystem::path assetsRoot{};
	ShaderSourcePolicy shaderSourcePolicy = ShaderSourcePolicy::PreferPrecompiled;
};
