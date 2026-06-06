#include "Engine/Renderer/Texture/Loader/TextureImageToUpload.h"

TextureUploadDesc BuildTextureUploadDesc(
	const TextureImageData& image,
	const char* debugName)
{
	TextureUploadDesc uploadDesc{};
	uploadDesc.width = image.width;
	uploadDesc.height = image.height;
	uploadDesc.mipLevels = image.mipCount;
	uploadDesc.format = image.format;
	uploadDesc.DebugName = debugName;

	uploadDesc.mips.reserve(image.mips.size());
	for (const TextureImageMip& sourceMip : image.mips)
	{
		TextureMipData mip{};
		mip.width = sourceMip.width;
		mip.height = sourceMip.height;
		mip.rowPitch = sourceMip.rowPitch;
		mip.pixels = sourceMip.pixels;
		uploadDesc.mips.push_back(mip);
	}

	return uploadDesc;
}
