#include "Engine/RHI/DX12/Debug/DX12Pix.h"

#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"

#if defined(AETHER_PIX) && AETHER_PIX && defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG

#ifndef USE_PIX
#define USE_PIX
#endif

#include "Engine/RHI/DX12/pch.h"

#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/LogCommon.h"
#include "Engine/Core/Log/LogMacros.h"

#include <pix3.h>

namespace
{

bool g_pixMarkersEnabled = false;

} // namespace

namespace DX12Pix
{

void Initialize()
{
	g_pixMarkersEnabled = false;

	const DX12DebugSettingsData& settings = DX12DebugSettingsData::Get();
	if (!settings.usePixMarkers)
	{
		return;
	}

	g_pixMarkersEnabled = true;

	if (!settings.loadPixGpuCapturer)
	{
		LOG_INFO(LogCategory::RHI, "PIX event runtime enabled (GPU markers via WinPixEventRuntime)");
		return;
	}

	if (PIXLoadLatestWinPixGpuCapturerLibrary() != nullptr)
	{
		LOG_INFO(LogCategory::RHI, "PIX GPU capturer library loaded");
	}
	else
	{
		LOG_WARN(
			LogCategory::RHI,
			"PIX GPU capturer not found. Install PIX for programmatic GPU capture; markers still work.");
	}
}

bool IsEnabled()
{
	return g_pixMarkersEnabled;
}

void BeginEvent(ID3D12GraphicsCommandList* commandList, const char* name)
{
	if (!g_pixMarkersEnabled || commandList == nullptr || name == nullptr || name[0] == '\0')
	{
		return;
	}

	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "%s", name);
}

void EndEvent(ID3D12GraphicsCommandList* commandList)
{
	if (!g_pixMarkersEnabled || commandList == nullptr)
	{
		return;
	}

	PIXEndEvent(commandList);
}

} // namespace DX12Pix

#else

namespace DX12Pix
{

void Initialize()
{
}

bool IsEnabled()
{
	return false;
}

void BeginEvent(ID3D12GraphicsCommandList* commandList, const char* name)
{
	(void)commandList;
	(void)name;
}

void EndEvent(ID3D12GraphicsCommandList* commandList)
{
	(void)commandList;
}

} // namespace DX12Pix

#endif
