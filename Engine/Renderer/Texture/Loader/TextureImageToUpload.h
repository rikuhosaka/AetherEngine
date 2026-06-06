#pragma once

#include "Engine/Renderer/Texture/Loader/TextureLoadTypes.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

// Spans in the returned desc reference memory owned by image. image must outlive the upload.
[[nodiscard]] TextureUploadDesc BuildTextureUploadDesc(
	const TextureImageData& image,
	const char* debugName = nullptr);
