#include "Runtime/Renderer/ShaderSystem/Loader/ShaderBytecodeFileLoader.h"

#include "Runtime/Core/IO/BinaryFile.h"

#include <cstring>

Result<ShaderBytecode> LoadShaderBytecodeFromFile(const std::filesystem::path& filePath)
{
	if (filePath.empty())
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Shader bytecode file path is empty.");
	}

	const Result<std::string> bytes = ReadBinaryFile(filePath);
	if (!bytes)
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Failed to open shader bytecode file: " + filePath.string());
	}

	if (bytes.value.empty())
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"Shader bytecode file is empty: " + filePath.string());
	}

	ShaderBytecode bytecode{};
	bytecode.Data.resize(bytes.value.size());
	std::memcpy(bytecode.Data.data(), bytes.value.data(), bytes.value.size());
	return MakeOk(std::move(bytecode));
}
