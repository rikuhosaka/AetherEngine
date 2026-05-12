#pragma once
#include <Engine/RHI/Interface/RHITexture.h>


class DX12Device;
class DX12SwapChain;
class ResourceImpl;

class DX12Texture : public RHITexture
{
public:
	~DX12Texture() override;

	uint32_t GetWidth() const override { return m_desc.Width; }
	uint32_t GetHeight() const override { return m_desc.Height; }
	uint32_t GetMipLevels() const override { return m_desc.MipLevels; }
	uint32_t GetArraySize() const override { return m_desc.ArraySize; }
	uint32_t GetSampleCount() const override { return m_desc.SampleCount; }
	uint32_t GetSampleQuality() const override { return m_desc.SampleQuality; }

	void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) override;
private:

	DX12Texture(const RHITextureDesc& desc, const DX12Device* dxDevice);
	DX12Texture(const RHITextureDesc& desc, std::unique_ptr<ResourceImpl> resource);

	static std::unique_ptr<DX12Texture> Create(const RHITextureDesc& desc, const DX12Device* dxDevice)
	{
		return std::make_unique<DX12Texture>(desc, dxDevice);
	}

	static std::unique_ptr<DX12Texture> Create(const RHITextureDesc& desc, std::unique_ptr<ResourceImpl> resource)
	{
		return std::make_unique<DX12Texture>(desc, std::move(resource));
	}

	RHITextureDesc m_desc;

	std::unique_ptr<ResourceImpl> m_impl = nullptr;

	ResourceImpl* GetResourceImpl() const
	{
		return m_impl.get();
	}

	friend class DX12SwapChain;
	friend class DX12Device;
};