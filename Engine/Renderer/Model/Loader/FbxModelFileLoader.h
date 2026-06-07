#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Model/Loader/FbxSdkContext.h"
#include "Engine/Renderer/Model/Loader/ModelLoadTypes.h"

#include <filesystem>

[[nodiscard]] Result<ModelAssetData> LoadModelAssetDataFromFile(
	const std::filesystem::path& absolutePath,
	FbxSdkContext& sdkContext,
	const FbxModelLoadOptions& options = {});
