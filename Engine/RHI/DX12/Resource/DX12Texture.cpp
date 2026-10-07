#include "DX12Texture.h"

#include "Engine/RHI/DX12/Common/DX12Format.h"
#include "Engine/RHI/DX12/Common/DX12Flag.h"
#include "Engine/RHI/DX12/Common/DX12Resource.h"
#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Resource/ResourceImpl.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"

#include <format>

bool DX12Texture::IsValid() const
{
	return m_impl != nullptr && m_impl->resource != nullptr;
}

Result<std::unique_ptr<DX12Texture>> DX12Texture::Create(const RHITextureDesc& desc, const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12Texture>(new DX12Texture(desc, dxDevice)),
		"Failed to create texture");
}

Result<std::unique_ptr<DX12Texture>> DX12Texture::Create(
	const RHITextureDesc& desc,
	std::unique_ptr<ResourceImpl> resource)
{
	return MakeResourceResult(
		std::unique_ptr<DX12Texture>(new DX12Texture(desc, std::move(resource))),
		"Failed to create texture from resource");
}

DX12Texture::DX12Texture(const RHITextureDesc& desc, const DX12Device* dxDevice)
	: m_desc(desc)
	, m_impl(std::make_unique<ResourceImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	ComPtr<ID3D12Resource> texture;
	const DXGI_FORMAT dxgiFormat = ToDxgiFormat(desc.Format);

    D3D12_RESOURCE_FLAGS flag = ToResourceFlags(desc.Usage);

	D3D12_RESOURCE_STATES initialState = ToResourceState(ERHIResourceState::CopyDest);
	m_impl->SetInitialState(ERHIResourceState::CopyDest);

    D3D12_CLEAR_VALUE clearValue{};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;
    if (desc.Usage == ERHITextureUsage::RenderTarget)
    {
		initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_impl->SetInitialState(ERHIResourceState::RenderTarget);
        clearValue.Format = dxgiFormat;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 0.0f;
    }
	if (desc.Usage == ERHITextureUsage::DepthStencil)
    {
		initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		m_impl->SetInitialState(ERHIResourceState::DepthWrite);
        clearValue.Format = ToDxgiFormat(GetDepthStencilViewFormat());
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        pClearValue = &clearValue;
    }
	const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		dxgiFormat,
		desc.Width,
		desc.Height,
		static_cast<UINT16>(desc.ArraySize),
		static_cast<UINT16>(desc.MipLevels),
		1,
		0,
		flag,
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		0
	);
	const HRESULT result = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		pClearValue,
		IID_PPV_ARGS(&texture));
	if (FAILED(result))
	{
		return;
	}
	m_impl->resource = texture;

	if (desc.DebugName != nullptr && desc.DebugName[0] != '\0')
	{
		DX12GpuNaming::SetResourceName(texture.Get(), "Tex", desc.DebugName);
	}
	else
	{
		DX12GpuNaming::SetName(
			texture.Get(),
			std::format("Tex/{}x{}", desc.Width, desc.Height));
	}
}

DX12Texture::DX12Texture(const RHITextureDesc& desc, std::unique_ptr<ResourceImpl> resource)
	: m_desc(desc)
	, m_impl(std::move(resource))
{
	if (m_impl != nullptr && m_impl->resource != nullptr)
	{
		if (desc.DebugName != nullptr && desc.DebugName[0] != '\0')
		{
			DX12GpuNaming::SetResourceName(m_impl->resource.Get(), "Tex", desc.DebugName);
		}
	}
}

ERHIFormat DX12Texture::GetDepthStencilViewFormat() const
{
	if (m_desc.depthStencilViewFormat != ERHIFormat::Unknown)
	{
		return m_desc.depthStencilViewFormat;
	}
	return m_desc.Format;
}

ERHIFormat DX12Texture::GetShaderResourceViewFormat() const
{
	if (m_desc.shaderResourceViewFormat != ERHIFormat::Unknown)
	{
		return m_desc.shaderResourceViewFormat;
	}
	return m_desc.Format;
}

DX12Texture::~DX12Texture()
{
	if (m_impl && m_impl->resource)
	{
		m_impl->resource.Reset();
	}
}

void DX12Texture::TransitionResource(ERHIResourceState newState, const RHICommandList* rhiCommandList)
{
	m_impl->TransitionResource(newState, rhiCommandList);
}
