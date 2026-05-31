#include "Game/DefaultGameModule.h"

#include "Game/IGameHost.h"

#include "Engine/Renderer/Scene/RenderSceneTypes.h"

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

void DefaultGameModule::OnTick(IGameHost& /*host*/, float /*deltaSeconds*/)
{
}

void DefaultGameModule::OnExtract(IGameHost& /*host*/, std::vector<ExtractedObject>& outObjects)
{
	ExtractedObject object{};
	object.objectId.Index = 0;
	object.objectId.Generation = 1;
	MakeIdentityMatrix(object.worldMatrix);
	object.layerMask = RenderLayer::Opaque;
	object.visible = true;
	outObjects.push_back(object);
}

void DefaultGameModule::OnShutdown(IGameHost& /*host*/)
{
}

IGameModule* CreateGameModule()
{
	return new DefaultGameModule();
}
