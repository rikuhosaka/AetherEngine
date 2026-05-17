#pragma once

#include "Engine/Renderer/ShaderSystem/Internal/IShaderCompilerBackend.h"

#include <filesystem>

class DxcShaderCompilerBackend final : public IShaderCompilerBackend
{
public:
	explicit DxcShaderCompilerBackend(std::filesystem::path dxcExecutable, std::filesystem::path scratchDirectory);

	[[nodiscard]] bool Compile(const ShaderCompileJob& job, ShaderCompileOutput& out) override;

private:
	std::filesystem::path m_dxcExecutable{};
	std::filesystem::path m_scratchDirectory{};
};
