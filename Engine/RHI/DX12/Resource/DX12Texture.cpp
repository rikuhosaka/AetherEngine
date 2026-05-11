#include "DX12Texture.h"
#include <Engine/RHI/DX12/Device/DX12Device.h>
#include <Engine/RHI/DX12/Device/DeviceImpl.h>
#include <Engine/RHI/DX12/Resource/ResourceImpl.h>


DX12Texture::DX12Texture(const RHITextureDesc& desc, const DX12Device* dxDevice)
	: m_desc(desc), m_impl(std::make_unique<ResourceImpl>())
{
	// DirectX 12テクスチャの作成コードをここに記述
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	ComPtr<ID3D12Resource> texture;
	auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, desc.Width, desc.Height, desc.ArraySize, desc.MipLevels);
	auto result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture)
	);
	if (FAILED(result)) {
		LOG_FATAL("Failed to create texture");
		return;
	}
	m_impl->resource = texture;
}

DX12Texture::DX12Texture(const RHITextureDesc& desc, std::unique_ptr<ResourceImpl> resource)
	: m_desc(desc), m_impl(std::move(resource))
{
}

DX12Texture::~DX12Texture()
{
	if (m_impl->resource)
	{
		m_impl->resource->Release();
		m_impl->resource = nullptr;
	}
}

void
DX12Texture::TransitionResource(ERHIResourceState newState, const RHICommandList* rhiCommandList)
{
	m_impl->TransitionResource(newState, rhiCommandList);
}