#pragma once

#include "Engine/Core/Handle/Handle.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Engine/Renderer/ShaderSystem/Compiler/ShaderType.h"
#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"

#include <cstdint>

struct ShaderCompileCacheKey
{
	std::uint64_t SourceFingerprint{};
	std::uint64_t DescFingerprint{};
};

struct ShaderReflectionCacheKey
{
	std::uint64_t BytecodeFingerprint{};
	ShaderStage Stage{ ShaderStage::Unknown };
};

struct ShaderBytecodeEntry
{
	ShaderCompileDesc Desc{};
	ShaderBytecode Bytecode{};
};

struct ShaderReflectionEntry
{
	ShaderReflectionData Data{};
};

using ShaderBytecodeHandle = Handle<ShaderBytecodeEntry>;
using ShaderReflectionHandle = Handle<ShaderReflectionEntry>;
