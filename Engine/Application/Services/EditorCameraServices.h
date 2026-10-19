#pragma once

#include "Engine/Renderer/Scene/RenderViewTypes.h"

#include <DirectXMath.h>

// Published by the editor. The game copies this pose when play starts.
struct EditorCameraServices
{
	bool valid = false;
	DirectX::XMFLOAT3 position{ 0.0f, 1.6f, -4.0f };
	DirectX::XMFLOAT3 forward{ 0.0f, 0.0f, 1.0f };
	ExtractedView view{};
};
