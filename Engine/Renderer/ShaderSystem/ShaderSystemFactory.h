#pragma once

#include "Engine/Renderer/ShaderSystem/IShaderSystem.h"
#include "Engine/Renderer/ShaderSystem/ShaderSystemSettings.h"

#include <memory>

class ShaderCatalog;

[[nodiscard]] std::unique_ptr<ShaderCatalog> CreateDefaultShaderCatalog(const ShaderSystemSettings& settings);

[[nodiscard]] std::unique_ptr<IShaderSystem> CreateShaderSystem(
	const ShaderSystemSettings& settings,
	const ShaderCatalog& catalog);

[[nodiscard]] std::filesystem::path ResolveDefaultDxcExecutable();
