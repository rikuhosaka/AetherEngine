#pragma once

#include <filesystem>

struct DX12DebugSettingsData
{
	bool enableDebugLayer = true;
	bool gpuBasedValidation = false;
	bool enableDred = true;
	bool enableGpuMarkers = true;
	bool usePixMarkers = true;
	bool loadPixGpuCapturer = false;
	bool enableFrameBarrierReport = false;
	bool logInfoQueueVerbose = false;

	static DX12DebugSettingsData Defaults();
	static const DX12DebugSettingsData& Get();
	static void LoadFromFile(const std::filesystem::path& path);
};
