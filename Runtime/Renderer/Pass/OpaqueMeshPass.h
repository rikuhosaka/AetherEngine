#pragma once

#include "Runtime/Renderer/Resource/FrameContext.h"
#include "Runtime/Renderer/RenderItem/RenderItem.h"
#include "Runtime/Renderer/Scene/RenderSceneTypes.h"

#include <span>

class MaterialSystemServices;
class MeshSystemServices;
class PipelineStateCache;
class RootSignatureCache;
class RHICommandList;
class RHITexture;

class OpaqueMeshPass
{
public:
	static void Execute(
		FrameContext& frameContext,
		RHICommandList* commandList,
		const RenderFrameSnapshot& snapshot,
		std::span<const RenderItem> items,
		MeshSystemServices& meshServices,
		MaterialSystemServices& materialServices,
		RootSignatureCache& rootSignatureCache,
		PipelineStateCache& pipelineStateCache,
		RHITexture* shadowMap);
};
