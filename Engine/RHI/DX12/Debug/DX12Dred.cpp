#include "Engine/RHI/DX12/pch.h"
#include "Engine/RHI/DX12/Debug/DX12Dred.h"
#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"

#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/LogCommon.h"
#include "Engine/Core/Log/LogMacros.h"

#include <format>
#include <string>

namespace
{

std::string WideToUtf8(const wchar_t* wide)
{
	if (wide == nullptr || wide[0] == L'\0')
	{
		return {};
	}

	const int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
	if (size <= 1)
	{
		return {};
	}

	std::string utf8(static_cast<size_t>(size - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), size, nullptr, nullptr);
	return utf8;
}

std::string GetDebugName(const char* nameA, const wchar_t* nameW)
{
	if (nameA != nullptr && nameA[0] != '\0')
	{
		return nameA;
	}

	return WideToUtf8(nameW);
}

std::string ToString(D3D12_AUTO_BREADCRUMB_OP op)
{
	switch (op)
	{
	case D3D12_AUTO_BREADCRUMB_OP_SETMARKER: return "SetMarker";
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED: return "DrawInstanced";
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED: return "DrawIndexedInstanced";
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT: return "ExecuteIndirect";
	case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER: return "ResourceBarrier";
	case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION: return "CopyBufferRegion";
	case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION: return "CopyTextureRegion";
	case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE: return "CopyResource";
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCH: return "Dispatch";
	case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW: return "ClearRenderTargetView";
	case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW: return "ClearDepthStencilView";
	case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1: return "SetPipelineState1";
	case D3D12_AUTO_BREADCRUMB_OP_BARRIER: return "Barrier";
	case D3D12_AUTO_BREADCRUMB_OP_BEGIN_COMMAND_LIST: return "BeginCommandList";
	default:
		return std::format("Op{}", static_cast<uint32_t>(op));
	}
}

const char* ToString(D3D12_DRED_ALLOCATION_TYPE type)
{
	switch (type)
	{
	case D3D12_DRED_ALLOCATION_TYPE_RESOURCE: return "Resource";
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST: return "CommandList";
	case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE: return "CommandQueue";
	case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP: return "DescriptorHeap";
	case D3D12_DRED_ALLOCATION_TYPE_HEAP: return "Heap";
	case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE: return "PipelineState";
	case D3D12_DRED_ALLOCATION_TYPE_FENCE: return "Fence";
	default:
		return "Other";
	}
}

void LogBreadcrumbNode(const D3D12_AUTO_BREADCRUMB_NODE1* node, uint32_t nodeIndex)
{
	if (node == nullptr)
	{
		return;
	}

	const UINT lastIndex = node->pLastBreadcrumbValue != nullptr ? *node->pLastBreadcrumbValue : 0;
	const D3D12_AUTO_BREADCRUMB_OP lastOp =
		node->pCommandHistory != nullptr && lastIndex > 0
		? node->pCommandHistory[lastIndex - 1]
		: static_cast<D3D12_AUTO_BREADCRUMB_OP>(0);

	const std::string message = std::format(
		"[DRED] Breadcrumb[{}] queue='{}' list='{}' lastIndex={} lastOp={} ({})",
		nodeIndex,
		GetDebugName(node->pCommandQueueDebugNameA, node->pCommandQueueDebugNameW),
		GetDebugName(node->pCommandListDebugNameA, node->pCommandListDebugNameW),
		lastIndex,
		static_cast<uint32_t>(lastOp),
		ToString(lastOp).c_str());

	Logger::Instance().Write(
		LogCategory::RHI,
		LogLevel::Fatal,
		message,
		std::source_location::current());
}

void LogAllocationChain(
	const char* chainLabel,
	const D3D12_DRED_ALLOCATION_NODE1* head)
{
	uint32_t index = 0;
	for (const D3D12_DRED_ALLOCATION_NODE1* node = head; node != nullptr; node = node->pNext)
	{
		const std::string message = std::format(
			"[DRED] PageFault {}[{}] type={} name='{}'",
			chainLabel,
			index,
			ToString(node->AllocationType),
			GetDebugName(node->ObjectNameA, node->ObjectNameW));

		Logger::Instance().Write(
			LogCategory::RHI,
			LogLevel::Fatal,
			message,
			std::source_location::current());
		++index;
	}
}

#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG

void LogDredDetails(ID3D12Device* device)
{
	ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dred))))
	{
		return;
	}

	D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
	if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs)))
	{
		uint32_t nodeIndex = 0;
		for (const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
			node != nullptr;
			node = node->pNext)
		{
			LogBreadcrumbNode(node, nodeIndex++);
		}
	}

	D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault = {};
	if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault)))
	{
		const std::string pageFaultMessage = std::format(
			"[DRED] PageFault VA=0x{:016X}",
			pageFault.PageFaultVA);
		Logger::Instance().Write(
			LogCategory::RHI,
			LogLevel::Fatal,
			pageFaultMessage,
			std::source_location::current());

		LogAllocationChain("Existing", pageFault.pHeadExistingAllocationNode);
		LogAllocationChain("RecentFreed", pageFault.pHeadRecentFreedAllocationNode);
	}
}

#endif

} // namespace

namespace DX12Dred
{

void ConfigureDevice(ID3D12Device* device)
{
#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG
	if (device == nullptr || !DX12DebugSettingsData::Get().enableDred)
	{
		return;
	}

	ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dredSettings))))
	{
		dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		LOG_INFO(LogCategory::RHI, "D3D12 DRED auto-breadcrumbs and page fault reporting enabled");
		return;
	}

	ComPtr<ID3D12DeviceRemovedExtendedDataSettings> legacyDredSettings;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&legacyDredSettings))))
	{
		legacyDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		legacyDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		LOG_INFO(LogCategory::RHI, "D3D12 DRED (legacy settings) enabled");
	}
#else
	(void)device;
#endif
}

bool IsDeviceRemovedHresult(const HRESULT hr)
{
	return hr == DXGI_ERROR_DEVICE_REMOVED
		|| hr == DXGI_ERROR_DEVICE_RESET
		|| hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR
		|| hr == static_cast<HRESULT>(0x887A0005); // D3D12_ERROR_DEVICE_REMOVED
}

void ReportDeviceRemoved(ID3D12Device* device, const std::string_view context)
{
	if (device == nullptr)
	{
		return;
	}

	const HRESULT removedReason = device->GetDeviceRemovedReason();
	if (removedReason == S_OK)
	{
		return;
	}

	const std::string summary = std::format(
		"[DRED] Device removed during {} (reason=0x{:08X})",
		context,
		static_cast<uint32_t>(removedReason));
	Logger::Instance().Write(
		LogCategory::RHI,
		LogLevel::Fatal,
		summary,
		std::source_location::current());

#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG
	LogDredDetails(device);
#endif
}

void CheckHresult(ID3D12Device* device, const HRESULT hr, const std::string_view context)
{
	if (device == nullptr || !IsDeviceRemovedHresult(hr))
	{
		return;
	}

	ReportDeviceRemoved(device, context);
}

void CheckDeviceHealth(ID3D12Device* device, const std::string_view context)
{
	ReportDeviceRemoved(device, context);
}

} // namespace DX12Dred
