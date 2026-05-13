#include "DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Command/DX12CommandList.h"
#include "Engine/RHI/DX12/Command/DX12CommandQueue.h"
#include "Engine/RHI/DX12/Command/DX12SwapChain.h"
#include "Engine/RHI/DX12/Memory/DX12Buffer.h"
#include "Engine/RHI/DX12/Memory/DX12UploadBuffer.h"
#include "Engine/RHI/DX12/Resource/DX12Texture.h"
#include "Engine/RHI/DX12/Descriptor/DX12DescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12TransientDescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12DSVAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12RTVAllocator.h"
#include "Engine/RHI/DX12/Pipeline/DX12RootSignature.h"
#include "Engine/RHI/DX12/Pipeline/DX12PipelineState.h"
#include "Engine/RHI/DX12/Resource/DX12PixelShader.h"
#include "Engine/RHI/DX12/Resource/DX12VertexShader.h"
#include "Engine/RHI/DX12/Sync/DX12Fence.h"


DeviceImpl* 
DX12Device::GetImpl() const
{
	return m_impl.get();
}

DX12Device::DX12Device()
{
}

DX12Device::~DX12Device()
{
	if (m_impl)
	{
		m_impl->device = nullptr;
		m_impl = nullptr;
	}
}

void DX12Device::Initialize()
{
	// DirectX 12???f???o???C???X????????????????????R???[???h??????????????????????L???q
	// ????????AD3D12CreateDevice??????????????g???p??????????f???o???C???X??????????????????????????B
	UINT flagsDXGI = 0;
	//DirectX12????????????????
	//???t???B???[???`???????????????x????????????
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	ComPtr<IDXGIFactory6> dxgiFactory;
	auto result = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(result)) {
		LOG_FATAL("Failed to create DXGIFactory");
		return;
	}
	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back(tmpAdapter);
	}
	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	//Direct3D???f???o???C???X?????????????????
	ComPtr<ID3D12Device> device;
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&device)) == S_OK) {
			featureLevel = l;
			break;
		}
	}
	if (!device) {
		LOG_FATAL("Failed to create D3D12 device");
		return;
	}
	m_impl = std::make_unique<DeviceImpl>();
	m_impl->device = device;
	m_impl->factory = dxgiFactory;
	return;
}

// Resource Command Creation
std::unique_ptr<RHICommandList> DX12Device::CreateCommandList()
{
	return DX12CommandList::Create(this);
}

std::unique_ptr<RHICommandQueue> DX12Device::CreateCommandQueue()
{
	return DX12CommandQueue::Create(this);
}

std::unique_ptr<RHISwapChain> DX12Device::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height, const RHICommandQueue* commandQueue)
{
	return DX12SwapChain::Create(hwnd, width, height, static_cast<const DX12CommandQueue*>(commandQueue), this);
}

// Resource Creation
std::unique_ptr<RHIVertexBuffer> DX12Device::CreateVertexBuffer(const RHIBufferDesc& desc, uint32_t stride)
{
	return DX12VertexBuffer::Create(desc, stride, this);
}

std::unique_ptr<RHIIndexBuffer> DX12Device::CreateIndexBuffer(const RHIBufferDesc& desc, IndexFormat indexFormat)
{
	return DX12IndexBuffer::Create(desc, indexFormat, this);
}

std::unique_ptr<RHIConstantBuffer> DX12Device::CreateConstantBuffer(const RHIBufferDesc& desc)
{
	return DX12ConstantBuffer::Create(desc, this);
}

std::unique_ptr<RHIStructuredBuffer> DX12Device::CreateStructuredBuffer(const RHIBufferDesc& desc)
{
	return DX12StructuredBuffer::Create(desc, this);
}

std::unique_ptr<RHITexture> DX12Device::CreateTexture(const RHITextureDesc& desc)
{
	return DX12Texture::Create(desc, this);
}

// Descriptor Creation
std::unique_ptr<RHIDescriptorAllocator> DX12Device::CreateDescriptorAllocator(uint32_t numDescriptors)
{
	return DX12DescriptorAllocator::Create(numDescriptors, this);
}

std::unique_ptr<RHITransientDescriptorAllocator> DX12Device::CreateTransientDescriptorAllocator(uint32_t numDescriptors)
{
	return DX12TransientDescriptorAllocator::Create(numDescriptors, this);
}

std::unique_ptr<RHIUploadBuffer> DX12Device::CreateUploadBuffer(size_t capacityInBytes)
{
	return DX12UploadBuffer::Create(capacityInBytes, this);
}

std::unique_ptr<RHIDSVAllocator> DX12Device::CreateDSVAllocator(uint32_t numDescriptors)
{
	return DX12DSVAllocator::Create(numDescriptors, this);
}

std::unique_ptr<RHIRTVAllocator> DX12Device::CreateRTVAllocator(uint32_t numDescriptors)
{
	return DX12RTVAllocator::Create(numDescriptors, this);
}

// Pipeline Creation
std::unique_ptr<RHIRootSignature> DX12Device::CreateRootSignature()
{
	return DX12RootSignature::Create(this);
}


std::unique_ptr<RHIPipelineState>
DX12Device::CreatePipelineState(const RHIPipelineDesc& pipelineDesc)
{
	return DX12PipelineState::Create(pipelineDesc, this);
}

// Sync Creation
std::unique_ptr<RHIFence> 
DX12Device::CreateFence()
{
	return DX12Fence::Create(this);
}