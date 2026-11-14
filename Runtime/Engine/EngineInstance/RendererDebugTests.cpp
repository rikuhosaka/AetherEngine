#include "Runtime/Engine/EngineInstance/RendererDebugTests.h"

#include "Runtime/Renderer/Test/FrustumCullTest.h"
#include "Runtime/Renderer/Test/PrimitiveMeshTest.h"
#include "Runtime/Renderer/Test/RendererResourceSmokeTest.h"
#include "Runtime/Renderer/Test/ShaderBytecodeLoaderTest.h"
#include "Runtime/Renderer/Test/ShadowMapTest.h"
#include "Runtime/Renderer/Test/TextureFileLoaderTest.h"

Result<void> RunRendererDebugTests(
	const std::filesystem::path& compiledShaderRoot,
	const std::filesystem::path& assetsRoot)
{
	if (auto result = RunShaderBytecodeLoaderTests(compiledShaderRoot); !result)
	{
		return result;
	}

	if (auto result = RunTextureFileLoaderTests(assetsRoot); !result)
	{
		return result;
	}

	if (auto result = RunPrimitiveMeshTests(); !result)
	{
		return result;
	}

	if (auto result = RunRendererResourceSmokeLayoutTests(); !result)
	{
		return result;
	}

	if (auto result = RunFrustumCullTests(); !result)
	{
		return result;
	}

	if (auto result = RunShadowMapTests(); !result)
	{
		return result;
	}

	return MakeOk();
}
