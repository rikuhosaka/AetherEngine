#include "Game/DefaultGameModule.h"

#include "Game/IGameHost.h"

#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Core/Renderer.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"
#include "Engine/RHI/Interface/RHICommandList.h"

#include <cstring>

namespace
{
void MakeIdentityMatrix(float* outMatrix4x4)
{
	std::memset(outMatrix4x4, 0, sizeof(float) * 16);
	outMatrix4x4[0] = 1.0f;
	outMatrix4x4[5] = 1.0f;
	outMatrix4x4[10] = 1.0f;
	outMatrix4x4[15] = 1.0f;
}
} // namespace

Result<void> DefaultGameModule::OnInit(IGameHost& /*host*/)
{
	return MakeOk();
}

Result<void> DefaultGameModule::OnPrepareRender(
	IGameHost& host,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	RenderServices* renderServices = host.GetRenderServices();
	if (renderServices == nullptr || renderServices->renderer == nullptr)
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"DefaultGameModule requires RenderServices");
	}

	const std::filesystem::path shaderRoot = host.GetShaderRoot();
	return m_quad.EnsureInitialized(
		*renderServices->renderer,
		frameContext,
		commandList,
		shaderRoot,
		{ 1.0f, 0.2f, 0.2f, 1.0f }
	    );
}

void DefaultGameModule::OnTick(IGameHost& /*host*/, float /*deltaSeconds*/)
{
}

void DefaultGameModule::OnExtract(IGameHost& /*host*/, std::vector<ExtractedObject>& outObjects)
{
	if (!m_quad.IsReady())
	{
		return;
	}

	ExtractedObject object{};
	object.objectId.Index = 0;
	object.objectId.Generation = 1;
	MakeIdentityMatrix(object.worldMatrix);
	object.layerMask = RenderLayer::Opaque;
	object.visible = true;
	m_quad.FillExtractedObject(object);
	outObjects.push_back(object);
}

void DefaultGameModule::OnShutdown(IGameHost& /*host*/)
{
}

IGameModule* CreateGameModule()
{
	return new DefaultGameModule();
}
