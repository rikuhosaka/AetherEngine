#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dx12.h> // 必要ならd3dx12.h（ヘルパー）も
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <DirectXMath.h>

#include <wrl.h>    // Microsoft::WRL::ComPtr
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <unordered_map>
#include <span>

#include <Engine/Core/Log/LogMacros.h>

using namespace Microsoft::WRL; // ComPtrを簡単に使えるようにするため

