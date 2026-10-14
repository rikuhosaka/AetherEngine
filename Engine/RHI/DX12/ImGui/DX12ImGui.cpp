#include "Engine/RHI/DX12/ImGui/DX12ImGui.h"

#include "Engine/RHI/DX12/Command/CommandImpl.h"
#include "Engine/RHI/DX12/Command/DX12CommandList.h"
#include "Engine/RHI/DX12/Command/DX12CommandQueue.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/Platform/GameWindow.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

namespace
{
constexpr uint32_t kSrvDescriptorCount = 64;
constexpr DXGI_FORMAT kRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT kDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;

struct SrvDescriptorPool
{
	SIZE_T cpuStart = 0;
	UINT64 gpuStart = 0;
	UINT increment = 0;
	std::vector<UINT> freeIndices{};
};

void AllocateSrvDescriptor(
	ImGui_ImplDX12_InitInfo* info,
	D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
	D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
{
	auto* pool = static_cast<SrvDescriptorPool*>(info->UserData);
	if (pool == nullptr || pool->freeIndices.empty())
	{
		LOG_ERROR(LogCategory::RHI, "ImGui SRV descriptor heap is full");
		outCpu->ptr = 0;
		outGpu->ptr = 0;
		return;
	}

	const UINT index = pool->freeIndices.back();
	pool->freeIndices.pop_back();
	outCpu->ptr = pool->cpuStart + static_cast<SIZE_T>(index) * pool->increment;
	outGpu->ptr = pool->gpuStart + static_cast<UINT64>(index) * pool->increment;
}

void FreeSrvDescriptor(
	ImGui_ImplDX12_InitInfo* info,
	D3D12_CPU_DESCRIPTOR_HANDLE cpu,
	D3D12_GPU_DESCRIPTOR_HANDLE gpu)
{
	auto* pool = static_cast<SrvDescriptorPool*>(info->UserData);
	if (pool == nullptr || pool->increment == 0)
	{
		return;
	}

	const UINT cpuIndex = static_cast<UINT>((cpu.ptr - pool->cpuStart) / pool->increment);
	const UINT gpuIndex = static_cast<UINT>((gpu.ptr - pool->gpuStart) / pool->increment);
	if (cpuIndex != gpuIndex || cpuIndex >= kSrvDescriptorCount)
	{
		LOG_ERROR(LogCategory::RHI, "ImGui SRV descriptor free received an unknown handle");
		return;
	}

	pool->freeIndices.push_back(cpuIndex);
}
} // namespace

namespace
{
GameWindow::MessageReply HandleGameWindowMessage(const GameWindow::Message& message, void* userData)
{
	GameWindow::MessageReply reply{};
	auto* imgui = static_cast<DX12ImGui*>(userData);
	if (imgui == nullptr)
	{
		return reply;
	}

	reply.handled = imgui->HandleWindowMessage(
		message.hwnd,
		message.message,
		message.wparam,
		message.lparam,
		reply.result);
	return reply;
}
} // namespace

struct DX12ImGui::Impl
{
	ComPtr<ID3D12DescriptorHeap> heap{};
	SrvDescriptorPool descriptors{};
	HWND hookedHwnd = nullptr;
	bool backendsReady = false;
};

DX12ImGui::~DX12ImGui()
{
	Shutdown();
}

Result<std::unique_ptr<DX12ImGui>> DX12ImGui::Create(const DX12Device* device, const RHIImGuiDesc& desc)
{
	auto context = std::unique_ptr<DX12ImGui>(new DX12ImGui());
	const std::string error = context->Initialize(device, desc);
	if (!error.empty())
	{
		return FailResourceCreation<DX12ImGui>(error);
	}

	return MakeOk(std::move(context));
}

std::string DX12ImGui::Initialize(const DX12Device* device, const RHIImGuiDesc& desc)
{
	if (device == nullptr || device->GetImpl() == nullptr || device->GetImpl()->device == nullptr
		|| desc.hwnd == nullptr || desc.commandQueue == nullptr || desc.framesInFlight == 0)
	{
		return "ImGui create received an invalid argument";
	}

	if (ImGui::GetCurrentContext() != nullptr)
	{
		return "ImGui context already exists";
	}

	auto* dxQueue = static_cast<const DX12CommandQueue*>(desc.commandQueue);
	if (dxQueue->GetImpl() == nullptr || dxQueue->GetImpl()->commandQueue == nullptr)
	{
		return "ImGui create requires a command queue";
	}

	ID3D12Device* d3dDevice = device->GetImpl()->device.Get();
	m_impl = std::make_unique<Impl>();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = kSrvDescriptorCount;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	const HRESULT heapHr = d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_impl->heap));
	if (FAILED(heapHr) || m_impl->heap == nullptr)
	{
		m_impl.reset();
		return "Failed to create ImGui descriptor heap";
	}

	DX12GpuNaming::SetName(m_impl->heap.Get(), "Heap/ImGui/CBV_SRV_UAV");

	SrvDescriptorPool& pool = m_impl->descriptors;
	pool.cpuStart = m_impl->heap->GetCPUDescriptorHandleForHeapStart().ptr;
	pool.gpuStart = m_impl->heap->GetGPUDescriptorHandleForHeapStart().ptr;
	pool.increment = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	pool.freeIndices.reserve(kSrvDescriptorCount);
	for (UINT index = kSrvDescriptorCount; index > 0; --index)
	{
		pool.freeIndices.push_back(index - 1);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;

	if (!ImGui_ImplWin32_Init(desc.hwnd))
	{
		ImGui::DestroyContext();
		m_impl.reset();
		return "Failed to initialize ImGui Win32 backend";
	}

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = d3dDevice;
	initInfo.CommandQueue = dxQueue->GetImpl()->commandQueue.Get();
	initInfo.NumFramesInFlight = static_cast<int>(desc.framesInFlight);
	initInfo.RTVFormat = kRenderTargetFormat;
	initInfo.DSVFormat = kDepthStencilFormat;
	initInfo.SrvDescriptorHeap = m_impl->heap.Get();
	initInfo.UserData = &m_impl->descriptors;
	initInfo.SrvDescriptorAllocFn = &AllocateSrvDescriptor;
	initInfo.SrvDescriptorFreeFn = &FreeSrvDescriptor;

	if (!ImGui_ImplDX12_Init(&initInfo))
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		m_impl.reset();
		return "Failed to initialize ImGui DX12 backend";
	}

	m_impl->backendsReady = true;

	if (GameWindow* window = GameWindow::FromHwnd(desc.hwnd))
	{
		window->SetMessageHook(&HandleGameWindowMessage, this);
		m_impl->hookedHwnd = desc.hwnd;
	}
	else
	{
		LOG_ERROR(LogCategory::RHI, "ImGui window messages require a GameWindow");
	}

	return {};
}

void DX12ImGui::NewFrame()
{
	if (m_impl == nullptr || !m_impl->backendsReady)
	{
		return;
	}

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void DX12ImGui::Render()
{
	if (m_impl == nullptr || !m_impl->backendsReady)
	{
		return;
	}

	ImGui::Render();
}

void DX12ImGui::Draw(RHICommandList* commandList)
{
	if (m_impl == nullptr || !m_impl->backendsReady || commandList == nullptr)
	{
		return;
	}

	auto* dxCommandList = static_cast<DX12CommandList*>(commandList);
	if (!dxCommandList->IsRecording() || dxCommandList->GetImpl() == nullptr
		|| dxCommandList->GetImpl()->commandList == nullptr)
	{
		LOG_ERROR(LogCategory::RHI, "ImGui draw requires a recording command list");
		return;
	}

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData == nullptr)
	{
		return;
	}

	ID3D12GraphicsCommandList* nativeCommandList = dxCommandList->GetImpl()->commandList.Get();
	ID3D12DescriptorHeap* heaps[] = { m_impl->heap.Get() };
	nativeCommandList->SetDescriptorHeaps(1, heaps);
	ImGui_ImplDX12_RenderDrawData(drawData, nativeCommandList);
}

bool DX12ImGui::HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result)
{
	if (m_impl == nullptr || !m_impl->backendsReady || ImGui::GetCurrentContext() == nullptr)
	{
		return false;
	}

	const LRESULT backendResult = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
	if (backendResult != 0)
	{
		result = backendResult;
		return true;
	}

	const ImGuiIO& io = ImGui::GetIO();
	const bool captureWheel = io.WantCaptureMouse && (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL);
	const bool captureText = io.WantCaptureKeyboard
		&& (msg == WM_CHAR || (msg == WM_UNICHAR && wparam != UNICODE_NOCHAR));
	if (!captureWheel && !captureText)
	{
		return false;
	}

	result = 0;
	return true;
}

void DX12ImGui::Shutdown()
{
	if (m_impl == nullptr)
	{
		return;
	}

	if (GameWindow* window = GameWindow::FromHwnd(m_impl->hookedHwnd))
	{
		window->ClearMessageHook(this);
	}
	m_impl->hookedHwnd = nullptr;

	if (m_impl->backendsReady)
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		m_impl->backendsReady = false;
	}

	m_impl.reset();
}

bool DX12ImGui::WantCaptureMouse() const
{
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return false;
	}

	return ImGui::GetIO().WantCaptureMouse;
}

bool DX12ImGui::WantCaptureKeyboard() const
{
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return false;
	}

	return ImGui::GetIO().WantCaptureKeyboard;
}

bool DX12ImGui::IsValid() const
{
	return m_impl != nullptr && m_impl->backendsReady && m_impl->heap != nullptr;
}
