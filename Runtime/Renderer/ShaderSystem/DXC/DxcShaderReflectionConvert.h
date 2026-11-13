#pragma once

#include "Runtime/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderType.h"

struct ID3D12ShaderReflection;

[[nodiscard]] ShaderReflectionData ConvertD3D12ShaderReflection(
	ID3D12ShaderReflection* reflection,
	ShaderStage stage);
