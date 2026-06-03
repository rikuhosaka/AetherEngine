#include "Engine/RHI/DX12/pch.h"
#include "Engine/RHI/DX12/Debug/DX12Debug.h"

#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/LogCommon.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/RHI/DX12/Debug/DX12InfoQueue.h"

#include <format>
#include <string>

namespace
{

bool g_debugLayerEnabled = false;

std::string WideToUtf8(const std::wstring& wide)
{
	if (wide.empty())
	{
		return {};
	}

	const int size = WideCharToMultiByte(
		CP_UTF8,
		0,
		wide.c_str(),
		-1,
		nullptr,
		0,
		nullptr,
		nullptr);
	if (size <= 1)
	{
		return {};
	}

	std::string utf8(static_cast<size_t>(size - 1), '\0');
	WideCharToMultiByte(
		CP_UTF8,
		0,
		wide.c_str(),
		-1,
		utf8.data(),
		size,
		nullptr,
		nullptr);
	return utf8;
}

} // namespace

bool DX12Debug::IsEnabled()
{
	return g_debugLayerEnabled;
}

bool DX12Debug::Initialize(const DX12DebugSettingsData& settings)
{
	g_debugLayerEnabled = false;

#if !defined(AETHER_DX12_DEBUG) || !AETHER_DX12_DEBUG
	(void)settings;
	return true;
#else
	if (!settings.enableDebugLayer)
	{
		LOG_INFO(LogCategory::RHI, "D3D12 debug layer disabled by dx12_debug.json");
		return true;
	}

	ComPtr<ID3D12Debug> debug;
	if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
	{
		LOG_WARN(LogCategory::RHI,
			"D3D12 debug layer is unavailable. Install Graphics Tools (Windows optional feature).");
		return true;
	}

	debug->EnableDebugLayer();
	g_debugLayerEnabled = true;

	ComPtr<ID3D12Debug1> debug1;
	if (settings.gpuBasedValidation && SUCCEEDED(debug.As(&debug1)))
	{
		debug1->SetEnableGPUBasedValidation(TRUE);
		LOG_INFO(LogCategory::RHI, "D3D12 GPU-based validation enabled");
	}

	LOG_INFO(LogCategory::RHI, "D3D12 debug layer enabled");
	return true;
#endif
}

void DX12Debug::Shutdown()
{
	DX12InfoQueue::DetachDevice();
	DX12InfoQueue::DetachFactory();
	g_debugLayerEnabled = false;
}

uint32_t DX12Debug::GetDxgiFactoryFlags()
{
	return g_debugLayerEnabled ? DXGI_CREATE_FACTORY_DEBUG : 0u;
}

void DX12Debug::AttachFactory(IDXGIFactory6* factory)
{
	if (!g_debugLayerEnabled)
	{
		return;
	}

	DX12InfoQueue::AttachFactory(factory);
}

void DX12Debug::DetachFactory()
{
	DX12InfoQueue::DetachFactory();
}

void DX12Debug::AttachDevice(ID3D12Device* device)
{
	if (!g_debugLayerEnabled || device == nullptr)
	{
		return;
	}

	DX12InfoQueue::AttachDevice(device);
}

void DX12Debug::DetachDevice()
{
	DX12InfoQueue::DetachDevice();
}

void DX12Debug::LogAdapters(IDXGIFactory6* factory)
{
	if (factory == nullptr)
	{
		return;
	}

	ComPtr<IDXGIAdapter> adapter;
	for (UINT index = 0;
		factory->EnumAdapters(index, &adapter) != DXGI_ERROR_NOT_FOUND;
		++index)
	{
		DXGI_ADAPTER_DESC desc = {};
		if (FAILED(adapter->GetDesc(&desc)))
		{
			adapter.Reset();
			continue;
		}

		const std::string name = WideToUtf8(desc.Description);
		LOG_INFO(
			LogCategory::RHI,
			std::format(
				"DXGI adapter [{}]: {} (DedicatedVideoMemory={} MB)",
				index,
				name,
				desc.DedicatedVideoMemory / (1024 * 1024)));
		adapter.Reset();
	}
}
