#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/AssetSystem/Model/Loader/FbxSdkContext.h"
#include "Runtime/AssetSystem/Model/Loader/ModelLoadTypes.h"

#include <filesystem>

[[nodiscard]] Result<ModelAssetData> LoadModelAssetDataFromFile(
	const std::filesystem::path& absolutePath,
	FbxSdkContext& sdkContext,
	const FbxModelLoadOptions& options = {});
