#include "Engine/RHI/DX12/pch.h"
#include "Engine/RHI/DX12/Debug/DX12InfoQueue.h"

#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG

#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/LogCommon.h"
#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"

#include <d3d12sdklayers.h>
#include <dxgidebug.h>

#include <format>
#include <string>
#include <vector>

namespace
{

LogLevel ToLogLevel(D3D12_MESSAGE_SEVERITY severity)
{
	switch (severity)
	{
	case D3D12_MESSAGE_SEVERITY_CORRUPTION:
	case D3D12_MESSAGE_SEVERITY_ERROR:
		return LogLevel::Error;
	case D3D12_MESSAGE_SEVERITY_WARNING:
		return LogLevel::Warning;
	case D3D12_MESSAGE_SEVERITY_INFO:
	case D3D12_MESSAGE_SEVERITY_MESSAGE:
	default:
		return LogLevel::Info;
	}
}

LogLevel ToLogLevel(DXGI_INFO_QUEUE_MESSAGE_SEVERITY severity)
{
	switch (severity)
	{
	case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR:
		return LogLevel::Error;
	case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING:
		return LogLevel::Warning;
	case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO:
	case DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE:
	default:
		return LogLevel::Info;
	}
}

void LogD3D12Message(
	D3D12_MESSAGE_CATEGORY category,
	D3D12_MESSAGE_SEVERITY severity,
	D3D12_MESSAGE_ID id,
	const char* description)
{
	const LogLevel level = ToLogLevel(severity);
	if (level == LogLevel::Info && !DX12DebugSettingsData::Get().logInfoQueueVerbose)
	{
		return;
	}

	const std::string message = std::format(
		"[D3D12] category={} id={} {}",
		static_cast<uint32_t>(category),
		static_cast<uint32_t>(id),
		description != nullptr ? description : "");

	Logger::Instance().Write(
		LogCategory::RHI,
		level,
		message,
		std::source_location::current());
}

void LogDxgiMessage(const DXGI_INFO_QUEUE_MESSAGE& message)
{
	const LogLevel level = ToLogLevel(message.Severity);
	if (level == LogLevel::Info && !DX12DebugSettingsData::Get().logInfoQueueVerbose)
	{
		return;
	}

	const std::string text = std::format(
		"[DXGI] category={} id={} {}",
		static_cast<uint32_t>(message.Category),
		static_cast<uint32_t>(message.ID),
		message.pDescription != nullptr ? message.pDescription : "");

	Logger::Instance().Write(
		LogCategory::RHI,
		level,
		text,
		std::source_location::current());
}

void __stdcall OnD3D12Message(
	D3D12_MESSAGE_CATEGORY category,
	D3D12_MESSAGE_SEVERITY severity,
	D3D12_MESSAGE_ID id,
	LPCSTR description,
	void* /*context*/)
{
	LogD3D12Message(category, severity, id, description);
}

void ConfigureD3D12InfoQueue(ID3D12InfoQueue* infoQueue)
{
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

	D3D12_MESSAGE_SEVERITY storageSeverities[] = {
		D3D12_MESSAGE_SEVERITY_CORRUPTION,
		D3D12_MESSAGE_SEVERITY_ERROR,
		D3D12_MESSAGE_SEVERITY_WARNING,
	};

	D3D12_INFO_QUEUE_FILTER storageFilter = {};
	storageFilter.AllowList.NumSeverities = static_cast<UINT>(std::size(storageSeverities));
	storageFilter.AllowList.pSeverityList = storageSeverities;

	infoQueue->PushEmptyStorageFilter();
	infoQueue->AddStorageFilterEntries(&storageFilter);
}

void ConfigureDxgiInfoQueue(IDXGIInfoQueue* infoQueue)
{
	infoQueue->SetBreakOnSeverity(
		DXGI_DEBUG_ALL,
		DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,
		TRUE);

	DXGI_INFO_QUEUE_MESSAGE_SEVERITY storageSeverities[] = {
		DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,
		DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING,
	};

	DXGI_INFO_QUEUE_FILTER storageFilter = {};
	storageFilter.AllowList.NumSeverities = static_cast<UINT>(std::size(storageSeverities));
	storageFilter.AllowList.pSeverityList = storageSeverities;

	infoQueue->PushEmptyStorageFilter(DXGI_DEBUG_ALL);
	infoQueue->AddStorageFilterEntries(DXGI_DEBUG_ALL, &storageFilter);
}

void FlushDxgiMessages(IDXGIInfoQueue* infoQueue)
{
	const UINT64 messageCount = infoQueue->GetNumStoredMessages(DXGI_DEBUG_ALL);
	for (UINT64 messageIndex = 0; messageIndex < messageCount; ++messageIndex)
	{
		SIZE_T messageByteLength = 0;
		HRESULT hr = infoQueue->GetMessage(
			DXGI_DEBUG_ALL,
			messageIndex,
			nullptr,
			&messageByteLength);
		if (FAILED(hr) || messageByteLength == 0)
		{
			continue;
		}

		std::vector<std::byte> buffer(messageByteLength);
		auto* message = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(buffer.data());
		hr = infoQueue->GetMessage(
			DXGI_DEBUG_ALL,
			messageIndex,
			message,
			&messageByteLength);
		if (SUCCEEDED(hr))
		{
			LogDxgiMessage(*message);
		}
	}

	infoQueue->ClearStoredMessages(DXGI_DEBUG_ALL);
}

ComPtr<ID3D12InfoQueue1> g_d3d12InfoQueue;
DWORD g_d3d12MessageCallbackCookie = 0;
ComPtr<IDXGIInfoQueue> g_dxgiInfoQueue;

} // namespace

namespace DX12InfoQueue
{

void AttachDevice(ID3D12Device* device)
{
	if (device == nullptr || g_d3d12InfoQueue != nullptr)
	{
		return;
	}

	ComPtr<ID3D12InfoQueue> infoQueue;
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		return;
	}

	ConfigureD3D12InfoQueue(infoQueue.Get());

	ComPtr<ID3D12InfoQueue1> infoQueue1;
	if (SUCCEEDED(infoQueue.As(&infoQueue1)))
	{
		DWORD callbackCookie = 0;
		if (SUCCEEDED(infoQueue1->RegisterMessageCallback(
			OnD3D12Message,
			D3D12_MESSAGE_CALLBACK_FLAG_NONE,
			nullptr,
			&callbackCookie)))
		{
			g_d3d12MessageCallbackCookie = callbackCookie;
			g_d3d12InfoQueue = infoQueue1;
		}
	}
}

void DetachDevice()
{
	if (g_d3d12InfoQueue != nullptr && g_d3d12MessageCallbackCookie != 0)
	{
		g_d3d12InfoQueue->UnregisterMessageCallback(g_d3d12MessageCallbackCookie);
		g_d3d12MessageCallbackCookie = 0;
	}
	g_d3d12InfoQueue.Reset();
}

void AttachFactory(IDXGIFactory6* /*factory*/)
{
	if (g_dxgiInfoQueue != nullptr)
	{
		return;
	}

	ComPtr<IDXGIInfoQueue> infoQueue;
	if (FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&infoQueue))))
	{
		return;
	}

	ConfigureDxgiInfoQueue(infoQueue.Get());
	g_dxgiInfoQueue = infoQueue;
}

void DetachFactory()
{
	if (g_dxgiInfoQueue != nullptr)
	{
		FlushDxgiMessages(g_dxgiInfoQueue.Get());
		g_dxgiInfoQueue.Reset();
	}
}

void FlushPendingMessages()
{
	if (g_dxgiInfoQueue != nullptr)
	{
		FlushDxgiMessages(g_dxgiInfoQueue.Get());
	}
}

} // namespace DX12InfoQueue

#endif
