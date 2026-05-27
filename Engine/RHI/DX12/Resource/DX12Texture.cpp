#include "DX12Texture.h"
#include <Engine/RHI/DX12/Common/DX12Format.h>
#include <Engine/RHI/DX12/Device/DX12Device.h>
#include <Engine/RHI/DX12/Device/DeviceImpl.h>
#include <Engine/RHI/DX12/Resource/ResourceImpl.h>


DX12Texture::DX12Texture(const RHITextureDesc& desc, const DX12Device* dxDevice)
	: m_desc(desc), m_impl(std::make_unique<ResourceImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	ComPtr<ID3D12Resource> texture;
	const DXGI_FORMAT dxgiFormat = ToDxgiFormat(desc.Format);
	const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		dxgiFormat,
		desc.Width,
		desc.Height,
		static_cast<UINT16>(desc.ArraySize),
		static_cast<UINT16>(desc.MipLevels));
	const HRESULT result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture));
	if (FAILED(result))
	{
		LOG_FATAL("Failed to create texture");
		return;
	}
	m_impl->resource = texture;
	m_impl->SetInitialState(ERHIResourceState::CopyDest);
}

DX12Texture::DX12Texture(const RHITextureDesc& desc, std::unique_ptr<ResourceImpl> resource)
	: m_desc(desc), m_impl(std::move(resource))
{
}

std::unique_ptr<DX12Texture>
DX12Texture::Create(const RHITextureDesc& desc, std::unique_ptr<ResourceImpl> resource)
{
	return std::unique_ptr<DX12Texture>(new DX12Texture(desc, std::move(resource)));
}

DX12Texture::~DX12Texture()
{
	if (m_impl && m_impl->resource)
	{
		m_impl->resource.Reset();
	}
}

void
DX12Texture::TransitionResource(ERHIResourceState newState, const RHICommandList* rhiCommandList)
{
	m_impl->TransitionResource(newState, rhiCommandList);
}
