#include "Engine/Renderer/ShaderSystem/Loader/ShaderBytecodeFileLoader.h"

#include <fstream>

Result<ShaderBytecode> LoadShaderBytecodeFromFile(const std::filesystem::path& filePath)
{
	if (filePath.empty())
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Shader bytecode file path is empty.");
	}

	std::ifstream stream(filePath, std::ios::binary | std::ios::ate);
	if (!stream)
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Failed to open shader bytecode file: " + filePath.string());
	}

	const std::streamsize fileSize = stream.tellg();
	if (fileSize <= 0)
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"Shader bytecode file is empty: " + filePath.string());
	}

	ShaderBytecode bytecode{};
	bytecode.Data.resize(static_cast<std::size_t>(fileSize));

	stream.seekg(0, std::ios::beg);
	stream.read(reinterpret_cast<char*>(bytecode.Data.data()), fileSize);
	if (!stream)
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"Failed to read shader bytecode file: " + filePath.string());
	}

	return MakeOk(std::move(bytecode));
}
