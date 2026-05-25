#pragma once

#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"

struct ID3D12ShaderReflection;

[[nodiscard]] ShaderReflectionData ConvertD3D12ShaderReflection(
	ID3D12ShaderReflection* reflection,
	ShaderStage stage);
