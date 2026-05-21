#include "DX12RootSignature.h"

#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Pipeline/PipelineImpl.h"

namespace
{
	[[nodiscard]] D3D12_DESCRIPTOR_RANGE_TYPE ToDescriptorRangeType(RHIRootParamType type)
	{
		switch (type)
		{
		case RHIRootParamType::CBV_Table:
			return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case RHIRootParamType::SRV_Table:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case RHIRootParamType::UAV_Table:
			return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		default:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		}
	}

	void InitStaticSampler(CD3DX12_STATIC_SAMPLER_DESC& samplerDesc, RHISamplerBinding binding)
	{
		switch (binding)
		{
		case RHISamplerBinding::Linear:
			samplerDesc.Init(0);
			break;
		case RHISamplerBinding::Aniso:
			samplerDesc.Init(
				1,
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
			break;
		case RHISamplerBinding::Wrap:
			samplerDesc.Init(
				2,
				D3D12_FILTER_MIN_MAG_MIP_LINEAR,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP);
			break;
		}
	}

	[[nodiscard]] D3D12_ROOT_SIGNATURE_FLAGS ToRootSignatureFlags(RHIRootSignatureFlags flags)
	{
		D3D12_ROOT_SIGNATURE_FLAGS d3dFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(RHIRootSignatureFlags::AllowInputAssembler)) != 0)
		{
			d3dFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}
		return d3dFlags;
	}
}

RootSignatureImpl*
DX12RootSignature::GetImpl() const
{
	return m_impl.get();
}

DX12RootSignature::DX12RootSignature(const DX12Device* dxDevice, const RHIRootSignatureLayout& layout)
	: m_impl(std::make_unique<RootSignatureImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	if (device == nullptr)
	{
		LOG_FATAL("DX12 device is null.");
		return;
	}

	std::vector<CD3DX12_DESCRIPTOR_RANGE> descriptorRanges;
	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
	descriptorRanges.reserve(layout.parameters.size());
	rootParameters.resize(layout.parameters.size());

	for (std::size_t parameterIndex = 0; parameterIndex < layout.parameters.size(); ++parameterIndex)
	{
		const RHIRootParameterDesc& parameter = layout.parameters[parameterIndex];
		CD3DX12_ROOT_PARAMETER& rootParameter = rootParameters[parameterIndex];

		switch (parameter.kind)
		{
		case RHIRootParamType::Constants:
			rootParameter.InitAsConstants(
				parameter.constants.num32BitValues,
				parameter.constants.baseRegister,
				parameter.constants.space);
			break;

		case RHIRootParamType::CBV_Table:
		case RHIRootParamType::SRV_Table:
		case RHIRootParamType::UAV_Table:
		{
			CD3DX12_DESCRIPTOR_RANGE range;
			range.Init(
				ToDescriptorRangeType(parameter.kind),
				parameter.range.count,
				parameter.range.baseRegister,
				parameter.range.space);
			descriptorRanges.push_back(range);
			rootParameter.InitAsDescriptorTable(1, &descriptorRanges.back());
			break;
		}

		case RHIRootParamType::StaticSampler:
			LOG_WARN("StaticSampler should be listed in RHIRootSignatureLayout::staticSamplers.");
			break;
		}
	}

	std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers(layout.staticSamplers.size());
	for (std::size_t samplerIndex = 0; samplerIndex < layout.staticSamplers.size(); ++samplerIndex)
	{
		InitStaticSampler(staticSamplers[samplerIndex], layout.staticSamplers[samplerIndex].samplerBinding);
	}

	CD3DX12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.Init(
		static_cast<UINT>(rootParameters.size()),
		rootParameters.empty() ? nullptr : rootParameters.data(),
		static_cast<UINT>(staticSamplers.size()),
		staticSamplers.empty() ? nullptr : staticSamplers.data(),
		ToRootSignatureFlags(layout.flags));

	ComPtr<ID3DBlob> rootSigBlob;
	ComPtr<ID3DBlob> errorBlob;
	const HRESULT serializeResult = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);
	if (FAILED(serializeResult))
	{
		if (errorBlob != nullptr)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		LOG_FATAL("Failed to serialize root signature.");
		return;
	}

	ComPtr<ID3D12RootSignature> rootSignature;
	const HRESULT createResult = device->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()));
	if (FAILED(createResult))
	{
		LOG_FATAL("Failed to create root signature.");
		return;
	}

	m_impl->rootSignature = rootSignature;
}

DX12RootSignature::~DX12RootSignature() = default;
