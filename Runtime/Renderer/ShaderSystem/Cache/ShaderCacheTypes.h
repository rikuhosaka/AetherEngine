#pragma once

#include "Runtime/Core/Handle/Handle.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderBytecode.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderCompileDesc.h"
#include "Runtime/Renderer/ShaderSystem/Compiler/ShaderType.h"
#include "Runtime/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"

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
