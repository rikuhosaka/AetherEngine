#include "Engine/RHI/DX12/Debug/DX12GpuMarkers.h"

#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"
#include "Engine/RHI/DX12/Debug/DX12Pix.h"

#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG

#include "Engine/RHI/DX12/pch.h"

#include <string>

namespace
{

std::wstring Utf8ToWide(const char* utf8)
{
	if (utf8 == nullptr || utf8[0] == '\0')
	{
		return {};
	}

	const int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
	if (size <= 1)
	{
		return {};
	}

	std::wstring wide(static_cast<size_t>(size - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), size);
	return wide;
}

void BeginEventD3D12(ID3D12GraphicsCommandList* commandList, const char* name)
{
	const std::wstring wideName = Utf8ToWide(name);
	if (wideName.empty())
	{
		return;
	}

	const UINT dataSize = static_cast<UINT>((wideName.size() + 1) * sizeof(wchar_t));
	commandList->BeginEvent(0, wideName.c_str(), dataSize);
}

} // namespace

namespace DX12GpuMarkers
{

void BeginEvent(ID3D12GraphicsCommandList* commandList, const char* name)
{
	if (!DX12DebugSettingsData::Get().enableGpuMarkers || commandList == nullptr || name == nullptr)
	{
		return;
	}

	if (DX12Pix::IsEnabled())
	{
		DX12Pix::BeginEvent(commandList, name);
		return;
	}

	BeginEventD3D12(commandList, name);
}

void EndEvent(ID3D12GraphicsCommandList* commandList)
{
	if (!DX12DebugSettingsData::Get().enableGpuMarkers || commandList == nullptr)
	{
		return;
	}

	if (DX12Pix::IsEnabled())
	{
		DX12Pix::EndEvent(commandList);
		return;
	}

	commandList->EndEvent();
}

} // namespace DX12GpuMarkers

#else

namespace DX12GpuMarkers
{

void BeginEvent(ID3D12GraphicsCommandList* commandList, const char* name)
{
	(void)commandList;
	(void)name;
}

void EndEvent(ID3D12GraphicsCommandList* commandList)
{
	(void)commandList;
}

} // namespace DX12GpuMarkers

#endif
