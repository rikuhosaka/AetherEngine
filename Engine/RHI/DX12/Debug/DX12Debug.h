#pragma once

#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"

#include <cstdint>

struct ID3D12Device;
struct IDXGIFactory6;

class DX12Debug
{
public:
	static bool IsEnabled();

	static bool Initialize(const DX12DebugSettingsData& settings = DX12DebugSettingsData::Defaults());
	static void Shutdown();

	static uint32_t GetDxgiFactoryFlags();

	static void AttachFactory(IDXGIFactory6* factory);
	static void DetachFactory();

	static void AttachDevice(ID3D12Device* device);
	static void DetachDevice();

	static void LogAdapters(IDXGIFactory6* factory);
};
