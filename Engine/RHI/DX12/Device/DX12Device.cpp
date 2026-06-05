#include "DX12Device.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Command/DX12CommandList.h"
#include "Engine/RHI/DX12/Command/DX12CommandQueue.h"
#include "Engine/RHI/DX12/Command/DX12SwapChain.h"
#include "Engine/RHI/DX12/Memory/DX12Buffer.h"
#include "Engine/RHI/DX12/Memory/DX12UploadBuffer.h"
#include "Engine/RHI/DX12/Resource/DX12Texture.h"
#include "Engine/RHI/DX12/Resource/DX12VertexShader.h"
#include "Engine/RHI/DX12/Resource/DX12PixelShader.h"
#include "Engine/RHI/DX12/Common/DX12Format.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"
#include "Engine/RHI/DX12/Descriptor/DX12DescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12TransientDescriptorAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12DSVAllocator.h"
#include "Engine/RHI/DX12/Descriptor/DX12RTVAllocator.h"
#include "Engine/RHI/DX12/Pipeline/DX12RootSignature.h"
#include "Engine/RHI/DX12/Pipeline/DX12PipelineState.h"
#include "Engine/RHI/DX12/Sync/DX12Fence.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Debug/DX12Debug.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"
#include "Engine/RHI/DX12/Debug/DX12Dred.h"
#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"
#include "Engine/RHI/DX12/Debug/DX12Pix.h"

DeviceImpl* DX12Device::GetImpl() const
{
	return m_impl.get();
}

DX12Device::DX12Device() = default;

DX12Device::~DX12Device()
{
	DX12Debug::DetachDevice();
	DX12Debug::DetachFactory();
	if (m_impl)
	{
		m_impl->device = nullptr;
		m_impl->factory = nullptr;
		m_impl = nullptr;
	}
}

Result<void> DX12Device::Initialize()
{
	DX12Debug::Initialize(DX12DebugSettingsData::Get());
	DX12Pix::Initialize();

	const UINT flagsDXGI = DX12Debug::GetDxgiFactoryFlags();
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	ComPtr<IDXGIFactory6> dxgiFactory;
	auto result = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(result))
	{
		return FailRuntime(LogCategory::RHI, ErrorCode::DeviceLost,
			"Failed to create DXGIFactory");
	}
	std::vector<ComPtr<IDXGIAdapter>> adapters;
	ComPtr<IDXGIAdapter> tmpAdapter = nullptr;
	for (int i = 0; dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	if (DX12Debug::IsEnabled())
	{
		DX12Debug::LogAdapters(dxgiFactory.Get());
	}

	DX12Debug::AttachFactory(dxgiFactory.Get());

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}

	ComPtr<ID3D12Device> device;
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels)
	{
		if (D3D12CreateDevice(tmpAdapter.Get(), l, IID_PPV_ARGS(&device)) == S_OK)
		{
			featureLevel = l;
			break;
		}
	}
	if (!device)
	{
		return FailRuntime(LogCategory::RHI, ErrorCode::DeviceLost,
			"Failed to create D3D12 device");
	}
	m_impl = std::make_unique<DeviceImpl>();
	m_impl->device = device;
	m_impl->factory = dxgiFactory;

	DX12Debug::AttachDevice(device.Get());
	DX12Dred::ConfigureDevice(device.Get());
	DX12GpuNaming::SetName(device.Get(), "Device/D3D12");

	(void)featureLevel;
	return MakeOk();
}

Result<std::unique_ptr<RHICommandList>> DX12Device::CreateCommandList()
{
	return CastResourceResult<RHICommandList, DX12CommandList>(DX12CommandList::Create(this));
}

Result<std::unique_ptr<RHICommandQueue>> DX12Device::CreateCommandQueue()
{
	return CastResourceResult<RHICommandQueue, DX12CommandQueue>(DX12CommandQueue::Create(this));
}

Result<std::unique_ptr<RHISwapChain>> DX12Device::CreateSwapChain(
	HWND hwnd,
	uint32_t width,
	uint32_t height,
	uint32_t bufferCount,
	const RHICommandQueue* commandQueue)
{
	return CastResourceResult<RHISwapChain, DX12SwapChain>(
		DX12SwapChain::Create(
			hwnd,
			width,
			height,
			bufferCount,
			static_cast<const DX12CommandQueue*>(commandQueue),
			this));
}

Result<std::unique_ptr<RHIVertexBuffer>> DX12Device::CreateVertexBuffer(
	const RHIBufferDesc& desc,
	uint32_t stride)
{
	return CastResourceResult<RHIVertexBuffer, DX12VertexBuffer>(DX12VertexBuffer::Create(desc, stride, this));
}

Result<std::unique_ptr<RHIIndexBuffer>> DX12Device::CreateIndexBuffer(
	const RHIBufferDesc& desc,
	IndexFormat indexFormat)
{
	return CastResourceResult<RHIIndexBuffer, DX12IndexBuffer>(DX12IndexBuffer::Create(desc, indexFormat, this));
}

Result<std::unique_ptr<RHIConstantBuffer>> DX12Device::CreateConstantBuffer(const RHIBufferDesc& desc)
{
	return CastResourceResult<RHIConstantBuffer, DX12ConstantBuffer>(DX12ConstantBuffer::Create(desc, this));
}

Result<std::unique_ptr<RHIStructuredBuffer>> DX12Device::CreateStructuredBuffer(const RHIBufferDesc& desc)
{
	return CastResourceResult<RHIStructuredBuffer, DX12StructuredBuffer>(DX12StructuredBuffer::Create(desc, this));
}

Result<std::unique_ptr<RHITexture>> DX12Device::CreateTexture(const RHITextureDesc& desc)
{
	return CastResourceResult<RHITexture, DX12Texture>(DX12Texture::Create(desc, this));
}

Result<std::unique_ptr<RHIVertexShader>> DX12Device::CreateVertexShader(std::span<const std::byte> bytecode)
{
	return CastResourceResult<RHIVertexShader, DX12VertexShader>(DX12VertexShader::Create(bytecode));
}

Result<std::unique_ptr<RHIPixelShader>> DX12Device::CreatePixelShader(std::span<const std::byte> bytecode)
{
	return CastResourceResult<RHIPixelShader, DX12PixelShader>(DX12PixelShader::Create(bytecode));
}

Result<CbvSrvUavHandle> DX12Device::CreateShaderResourceView(
	RHITexture* texture,
	RHIDescriptorAllocator* allocator)
{
	if (texture == nullptr || allocator == nullptr)
	{
		return FailInternal<CbvSrvUavHandle>(LogCategory::RHI, ErrorCode::InvalidArgument,
			"Texture or allocator is null");
	}

	auto* dxTexture = static_cast<DX12Texture*>(texture);
	auto* dxAllocator = static_cast<DX12DescriptorAllocator*>(allocator);
	ResourceImpl* resource = dxTexture->GetResourceImpl();
	if (resource == nullptr || resource->resource == nullptr)
	{
		return FailRuntime<CbvSrvUavHandle>(LogCategory::RHI, ErrorCode::InvalidArgument,
			"Texture resource is invalid");
	}

	const uint32_t index = dxAllocator->Allocate();
	if (index == UINT32_MAX)
	{
		return MakeFail<CbvSrvUavHandle>(
			ErrorCode::ResourceCreationFailed,
			"Failed to allocate shader resource view descriptor");
	}

	const CpuDescHandle cpuHandle = dxAllocator->GetCpuHandle(index);
	const GpuDescHandle gpuHandle = dxAllocator->GetGpuHandle(index);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = ToDxgiFormat(dxTexture->GetFormat());
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = dxTexture->GetMipLevels();

	ID3D12Device* device = GetImpl()->device.Get();
	device->CreateShaderResourceView(
		resource->resource.Get(),
		&srvDesc,
		{ cpuHandle.ptr });

	return MakeOk(CbvSrvUavHandle{ cpuHandle, gpuHandle });
}

void DX12Device::WriteShaderResourceView(RHITexture* texture, CpuDescHandle destCpuHandle)
{
	if (texture == nullptr || destCpuHandle.ptr == 0)
	{
		return;
	}

	auto* dxTexture = static_cast<DX12Texture*>(texture);
	ResourceImpl* resource = dxTexture->GetResourceImpl();
	if (resource == nullptr || resource->resource == nullptr)
	{
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = ToDxgiFormat(dxTexture->GetFormat());
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = dxTexture->GetMipLevels();

	GetImpl()->device->CreateShaderResourceView(
		resource->resource.Get(),
		&srvDesc,
		{ destCpuHandle.ptr });
}

void DX12Device::WriteRenderTargetView(RHITexture* texture, RtvHandle dest)
{
	if (texture == nullptr || dest.cpu.ptr == 0)
	{
		return;
	}

	auto* dxTexture = static_cast<DX12Texture*>(texture);
	ResourceImpl* resource = dxTexture->GetResourceImpl();
	if (resource == nullptr || resource->resource == nullptr)
	{
		return;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = ToDxgiFormat(dxTexture->GetFormat());
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	GetImpl()->device->CreateRenderTargetView(
		resource->resource.Get(),
		&rtvDesc,
		{ dest.cpu.ptr });
}

void DX12Device::WriteDepthStencilView(RHITexture* texture, DsvHandle dest)
{
	if (texture == nullptr || dest.cpu.ptr == 0)
	{
		return;
	}

	auto* dxTexture = static_cast<DX12Texture*>(texture);
	ResourceImpl* resource = dxTexture->GetResourceImpl();
	if (resource == nullptr || resource->resource == nullptr)
	{
		return;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = ToDxgiFormat(dxTexture->GetFormat());
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	GetImpl()->device->CreateDepthStencilView(
		resource->resource.Get(),
		&dsvDesc,
		{ dest.cpu.ptr });
}

void DX12Device::WriteConstantBufferView(
	RHIBuffer* buffer,
	CpuDescHandle destCpuHandle,
	uint32_t bufferSizeInBytes)
{
	if (buffer == nullptr || destCpuHandle.ptr == 0 || bufferSizeInBytes == 0)
	{
		return;
	}

	ResourceImpl* resource = nullptr;
	if (auto* vertexBuffer = dynamic_cast<RHIVertexBuffer*>(buffer))
	{
		resource = static_cast<DX12VertexBuffer*>(vertexBuffer)->GetBufferResourceImpl();
	}
	else if (auto* indexBuffer = dynamic_cast<RHIIndexBuffer*>(buffer))
	{
		resource = static_cast<DX12IndexBuffer*>(indexBuffer)->GetBufferResourceImpl();
	}
	else if (auto* constantBuffer = dynamic_cast<RHIConstantBuffer*>(buffer))
	{
		resource = static_cast<DX12ConstantBuffer*>(constantBuffer)->GetBufferResourceImpl();
	}

	if (resource == nullptr || resource->resource == nullptr)
	{
		return;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = resource->resource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (bufferSizeInBytes + 255u) & ~255u;

	GetImpl()->device->CreateConstantBufferView(&cbvDesc, { destCpuHandle.ptr });
}

void DX12Device::WriteConstantBufferView(
	RHIUploadBuffer* upload,
	size_t offset,
	uint32_t sizeInBytes,
	CpuDescHandle destCpuHandle)
{
	if (upload == nullptr || destCpuHandle.ptr == 0 || sizeInBytes == 0)
	{
		return;
	}

	auto* dxUpload = static_cast<DX12UploadBuffer*>(upload);
	if (dxUpload->GetImpl()->resource == nullptr)
	{
		return;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = dxUpload->GetImpl()->resource->GetGPUVirtualAddress() + offset;
	cbvDesc.SizeInBytes = (sizeInBytes + 255u) & ~255u;

	GetImpl()->device->CreateConstantBufferView(&cbvDesc, { destCpuHandle.ptr });
}

Result<std::unique_ptr<RHIDescriptorAllocator>> DX12Device::CreateDescriptorAllocator(uint32_t numDescriptors)
{
	return CastResourceResult<RHIDescriptorAllocator, DX12DescriptorAllocator>(
		DX12DescriptorAllocator::Create(numDescriptors, this));
}

Result<std::unique_ptr<RHITransientDescriptorAllocator>> DX12Device::CreateTransientDescriptorAllocator(
	uint32_t numDescriptors)
{
	return CastResourceResult<RHITransientDescriptorAllocator, DX12TransientDescriptorAllocator>(
		DX12TransientDescriptorAllocator::Create(numDescriptors, this));
}

Result<std::unique_ptr<RHIUploadBuffer>> DX12Device::CreateUploadBuffer(size_t capacityInBytes)
{
	return CastResourceResult<RHIUploadBuffer, DX12UploadBuffer>(DX12UploadBuffer::Create(capacityInBytes, this));
}

Result<std::unique_ptr<RHIDSVAllocator>> DX12Device::CreateDSVAllocator(uint32_t numDescriptors)
{
	return CastResourceResult<RHIDSVAllocator, DX12DSVAllocator>(DX12DSVAllocator::Create(numDescriptors, this));
}

Result<std::unique_ptr<RHIRTVAllocator>> DX12Device::CreateRTVAllocator(uint32_t numDescriptors)
{
	return CastResourceResult<RHIRTVAllocator, DX12RTVAllocator>(DX12RTVAllocator::Create(numDescriptors, this));
}

Result<std::unique_ptr<RHIRootSignature>> DX12Device::CreateRootSignature(const RHIRootSignatureLayout& layout)
{
	return CastResourceResult<RHIRootSignature, DX12RootSignature>(DX12RootSignature::Create(this, layout));
}

Result<std::unique_ptr<RHIPipelineState>> DX12Device::CreatePipelineState(const RHIPipelineDesc& pipelineDesc)
{
	return CastResourceResult<RHIPipelineState, DX12PipelineState>(DX12PipelineState::Create(pipelineDesc, this));
}

Result<std::unique_ptr<RHIFence>> DX12Device::CreateFence()
{
	return CastResourceResult<RHIFence, DX12Fence>(DX12Fence::Create(this));
}
