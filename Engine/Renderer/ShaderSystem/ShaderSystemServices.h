#pragma once

#include "Engine/Renderer/ShaderSystem/Cache/ShaderBytecodeCache.h"
#include "Engine/Renderer/ShaderSystem/Cache/ShaderReflectionCache.h"

#include <memory>
#include <string>

class DxcShaderCompilerBackend;
class DxcShaderContext;
class DxcShaderReflectionBackend;
class IShaderCompilerBackend;
class IShaderReflectionBackend;

class ShaderSystemServices
{
public:
	[[nodiscard]] static std::unique_ptr<ShaderSystemServices> Create(std::string* outError = nullptr);

	[[nodiscard]] bool IsInitialized() const noexcept;

	[[nodiscard]] DxcShaderContext& GetContext() noexcept;
	[[nodiscard]] const DxcShaderContext& GetContext() const noexcept;

	[[nodiscard]] ShaderBytecodeCache& GetBytecodeCache() noexcept;
	[[nodiscard]] const ShaderBytecodeCache& GetBytecodeCache() const noexcept;

	[[nodiscard]] ShaderReflectionCache& GetReflectionCache() noexcept;
	[[nodiscard]] const ShaderReflectionCache& GetReflectionCache() const noexcept;

	[[nodiscard]] IShaderCompilerBackend& GetCompilerBackend() noexcept;
	[[nodiscard]] IShaderReflectionBackend& GetReflectionBackend() noexcept;

	~ShaderSystemServices();

	ShaderSystemServices() = default;

	bool m_initialized{ false };
	std::unique_ptr<DxcShaderContext> m_context{};
	std::unique_ptr<DxcShaderCompilerBackend> m_compilerBackend{};
	std::unique_ptr<DxcShaderReflectionBackend> m_reflectionBackend{};
	std::unique_ptr<ShaderBytecodeCache> m_bytecodeCache{};
	std::unique_ptr<ShaderReflectionCache> m_reflectionCache{};
};
