#pragma once

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Common/RHITexture.h"
#include "Engine/RHI/Interface/RHITexture.h"

#include <memory>

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

	ResourceImpl* GetResourceImpl() const { return m_impl.get(); }
	ERHIFormat GetFormat() const { return m_desc.Format; }
	[[nodiscard]] ERHIFormat GetDepthStencilViewFormat() const;
	[[nodiscard]] ERHIFormat GetShaderResourceViewFormat() const;

	[[nodiscard]] bool IsValid() const;

	static Result<std::unique_ptr<DX12Texture>> Create(const RHITextureDesc& desc, const DX12Device* dxDevice);
	static Result<std::unique_ptr<DX12Texture>> Create(
		const RHITextureDesc& desc,
		std::unique_ptr<ResourceImpl> resource);

private:
	DX12Texture(const RHITextureDesc& desc, const DX12Device* dxDevice);
	DX12Texture(const RHITextureDesc& desc, std::unique_ptr<ResourceImpl> resource);

	RHITextureDesc m_desc{};
	std::unique_ptr<ResourceImpl> m_impl{};

	friend class DX12SwapChain;
	friend class DX12Device;
};
