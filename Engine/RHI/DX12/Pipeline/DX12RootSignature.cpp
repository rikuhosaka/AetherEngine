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
		case RHIRootParamType::CBV:
			return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case RHIRootParamType::SRV:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case RHIRootParamType::UAV:
			return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		default:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		}
	}

	[[nodiscard]] D3D12_SHADER_VISIBILITY ToShaderVisibility(RHIShaderVisibility visibility)
	{
		switch (visibility)
		{
		case RHIShaderVisibility::Vertex:
			return D3D12_SHADER_VISIBILITY_VERTEX;
		case RHIShaderVisibility::Pixel:
			return D3D12_SHADER_VISIBILITY_PIXEL;
		case RHIShaderVisibility::Compute:
			return D3D12_SHADER_VISIBILITY_ALL; // D3D12 has no compute-only visibility on root params in 1.0 tables
		case RHIShaderVisibility::Geometry:
			return D3D12_SHADER_VISIBILITY_GEOMETRY;
		case RHIShaderVisibility::Hull:
			return D3D12_SHADER_VISIBILITY_HULL;
		case RHIShaderVisibility::Domain:
			return D3D12_SHADER_VISIBILITY_DOMAIN;
		case RHIShaderVisibility::All:
		default:
			return D3D12_SHADER_VISIBILITY_ALL;
		}
	}

	[[nodiscard]] D3D12_FILTER ToFilter(RHIFilterMode filter)
	{
		switch (filter)
		{
		case RHIFilterMode::Point:
			return D3D12_FILTER_MIN_MAG_MIP_POINT;
		case RHIFilterMode::Anisotropic:
			return D3D12_FILTER_ANISOTROPIC;
		case RHIFilterMode::Linear:
		default:
			return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		}
	}

	[[nodiscard]] D3D12_TEXTURE_ADDRESS_MODE ToAddressMode(RHIAddressMode address)
	{
		switch (address)
		{
		case RHIAddressMode::Clamp:
			return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case RHIAddressMode::Mirror:
			return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case RHIAddressMode::Border:
			return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		case RHIAddressMode::Wrap:
		default:
			return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		}
	}

	void InitStaticSampler(CD3DX12_STATIC_SAMPLER_DESC& samplerDesc, const RHIRootStaticSampler& sampler)
	{
		samplerDesc.Filter = ToFilter(sampler.filter);
		samplerDesc.AddressU = ToAddressMode(sampler.addressU);
		samplerDesc.AddressV = ToAddressMode(sampler.addressV);
		samplerDesc.AddressW = ToAddressMode(sampler.addressW);
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = (sampler.filter == RHIFilterMode::Anisotropic) ? 16 : 1;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = sampler.shaderRegister;
		samplerDesc.RegisterSpace = sampler.space;
		samplerDesc.ShaderVisibility = ToShaderVisibility(sampler.visibility);
	}

	[[nodiscard]] D3D12_ROOT_SIGNATURE_FLAGS ToRootSignatureFlags(RHIRootSignatureFlags flags)
	{
		D3D12_ROOT_SIGNATURE_FLAGS d3dFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(RHIRootSignatureFlags::AllowInputAssembler)) != 0)
		{
			d3dFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}
		if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(RHIRootSignatureFlags::DenyVertexShaderAccess)) != 0)
		{
			d3dFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
		}
		if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(RHIRootSignatureFlags::DenyPixelShaderAccess)) != 0)
		{
			d3dFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		}
#if defined(D3D12_ROOT_SIGNATURE_FLAG_DENY_COMPUTE_SHADER_ROOT_ACCESS)
		if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(RHIRootSignatureFlags::DenyComputeShaderAccess)) != 0)
		{
			d3dFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_COMPUTE_SHADER_ROOT_ACCESS;
		}
#endif
#ifdef D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
		if ((static_cast<uint32_t>(flags) & static_cast<uint32_t>(RHIRootSignatureFlags::DirectlyIndexedHeap)) != 0)
		{
			d3dFlags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		}
#endif

		return d3dFlags;
	}

	[[nodiscard]] UINT GetDescriptorCount(const RHIRootDescriptorRange& range)
	{
		return range.unbounded ? UINT32_MAX : range.count;
	}

	void InitRootDescriptor(
		CD3DX12_ROOT_PARAMETER& rootParameter,
		const RHIRootParameterDesc& parameter,
		D3D12_SHADER_VISIBILITY visibility)
	{
		const RHIRootDescriptor& descriptor = parameter.descriptor;
		const UINT registerIndex = descriptor.shaderRegister;
		const UINT registerSpace = descriptor.space;

		switch (parameter.kind)
		{
		case RHIRootParameterKind::RootCBV:
			rootParameter.InitAsConstantBufferView(registerIndex, registerSpace, visibility);
			break;
		case RHIRootParameterKind::RootSRV:
			rootParameter.InitAsShaderResourceView(registerIndex, registerSpace, visibility);
			break;
		case RHIRootParameterKind::RootUAV:
			rootParameter.InitAsUnorderedAccessView(registerIndex, registerSpace, visibility);
			break;
		default:
			break;
		}

		(void)descriptor.type;
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
		LOG_FATAL(LogCategory::RHI, "DX12 device is null.");
		return;
	}

	std::vector<std::vector<CD3DX12_DESCRIPTOR_RANGE>> descriptorRangeStorage;
	std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
	descriptorRangeStorage.reserve(layout.parameters.size());
	rootParameters.resize(layout.parameters.size());

	for (std::size_t parameterIndex = 0; parameterIndex < layout.parameters.size(); ++parameterIndex)
	{
		const RHIRootParameterDesc& parameter = layout.parameters[parameterIndex];
		CD3DX12_ROOT_PARAMETER& rootParameter = rootParameters[parameterIndex];
		const D3D12_SHADER_VISIBILITY visibility = ToShaderVisibility(parameter.visibility);

		switch (parameter.kind)
		{
		case RHIRootParameterKind::Constants:
			rootParameter.InitAsConstants(
				parameter.constants.num32BitValues,
				parameter.constants.baseRegister,
				parameter.constants.space,
				visibility);
			break;

		case RHIRootParameterKind::DescriptorTable:
		{
			std::vector<CD3DX12_DESCRIPTOR_RANGE> ranges;
			ranges.reserve(parameter.ranges.size());

			for (const RHIRootDescriptorRange& range : parameter.ranges)
			{
				CD3DX12_DESCRIPTOR_RANGE d3dRange;
				d3dRange.Init(
					ToDescriptorRangeType(range.type),
					GetDescriptorCount(range),
					range.baseRegister,
					range.space,
					range.offset);
				ranges.push_back(d3dRange);
			}

			descriptorRangeStorage.push_back(std::move(ranges));
			const std::vector<CD3DX12_DESCRIPTOR_RANGE>& storedRanges = descriptorRangeStorage.back();
			rootParameter.InitAsDescriptorTable(
				static_cast<UINT>(storedRanges.size()),
				storedRanges.data(),
				visibility);
			break;
		}

		case RHIRootParameterKind::RootCBV:
		case RHIRootParameterKind::RootSRV:
		case RHIRootParameterKind::RootUAV:
			InitRootDescriptor(rootParameter, parameter, visibility);
			break;
		}
	}

	std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers(layout.staticSamplers.size());
	for (std::size_t samplerIndex = 0; samplerIndex < layout.staticSamplers.size(); ++samplerIndex)
	{
		InitStaticSampler(staticSamplers[samplerIndex], layout.staticSamplers[samplerIndex]);
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
		LOG_FATAL(LogCategory::RHI, "Failed to serialize root signature.");
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
		LOG_FATAL(LogCategory::RHI, "Failed to create root signature.");
		return;
	}

	m_impl->rootSignature = rootSignature;
}

DX12RootSignature::~DX12RootSignature() = default;
