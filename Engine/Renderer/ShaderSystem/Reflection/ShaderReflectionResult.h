#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"

struct ShaderReflectionResult
{
	bool Succeeded{ false };

	ShaderReflectionData Data{};

	std::string Errors{};
};
