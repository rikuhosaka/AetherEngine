#pragma once

#include "Engine/Renderer/ShaderSystem/ShaderBytecodeView.h"
#include "Engine/Renderer/ShaderSystem/ShaderStage.h"

#include <array>
#include <optional>

struct ShaderProgramBundle
{
	std::array<std::optional<ShaderBytecodeView>, static_cast<std::size_t>(ShaderStage::Count)> Stages{};
};
