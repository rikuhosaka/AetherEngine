#include "Runtime/Launch/DebugStartupTests.h"

#include "Engine/Application/Test/FixedStepTest.h"
#include "Engine/Renderer/Test/FbxModelFileLoaderTest.h"
#include "Engine/Renderer/Test/FrustumCullTest.h"
#include "Engine/Renderer/Test/PrimitiveMeshTest.h"
#include "Engine/Renderer/Test/RendererResourceSmokeTest.h"
#include "Engine/Renderer/Test/ShaderBytecodeLoaderTest.h"
#include "Engine/Renderer/Test/ShadowMapTest.h"
#include "Engine/Renderer/Test/TextureFileLoaderTest.h"
#include "Engine/World/Test/WorldTest.h"
#include "Game/WorldExtractTest.h"

Result<void> RunDebugStartupTests(
	[[maybe_unused]] const std::filesystem::path& compiledShaderRoot,
	[[maybe_unused]] const std::filesystem::path& assetsRoot)
{
#ifdef _DEBUG
	if (auto result = RunShaderBytecodeLoaderTests(compiledShaderRoot); !result)
	{
		return result;
	}

	if (auto result = RunTextureFileLoaderTests(assetsRoot); !result)
	{
		return result;
	}

	if (auto result = RunFbxModelFileLoaderTests(assetsRoot); !result)
	{
		return result;
	}

	if (auto result = RunPrimitiveMeshTests(); !result)
	{
		return result;
	}

	if (auto result = RunWorldTests(); !result)
	{
		return result;
	}

	if (auto result = RunWorldExtractTests(); !result)
	{
		return result;
	}

	if (auto result = RunRendererResourceSmokeLayoutTests(); !result)
	{
		return result;
	}

	if (auto result = RunFixedStepTests(); !result)
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
#endif

	return MakeOk();
}
