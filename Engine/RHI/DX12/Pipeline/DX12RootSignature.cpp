#include "DX12RootSignature.h"
#include "Engine/RHI/DX12/Pipeline/PipelineImpl.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"


RootSignatureImpl*
DX12RootSignature::GetImpl() const
{
	return m_impl.get();
}

DX12RootSignature::DX12RootSignature(const DX12Device* dxDevice)
	: m_impl(std::make_unique<RootSignatureImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	using Range = CD3DX12_DESCRIPTOR_RANGE;
	using RootParam = CD3DX12_ROOT_PARAMETER;
	using SamplerDesc = CD3DX12_STATIC_SAMPLER_DESC;

	constexpr uint32_t ROOT_PARAM_COUNT = 7;
	constexpr uint32_t SAMPLER_COUNT = 3;
	constexpr uint32_t textureCount = 8; // 可変数テクスチャの数


	Range cbvEntity;     cbvEntity.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 3, 0); // b0
	Range cbvViewProj;   cbvViewProj.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // b1
	Range srvWorldMat;   srvWorldMat.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0
	Range srvBones;      srvBones.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1
	Range srvMaterial;   srvMaterial.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // t2
	// 可変数テクスチャ
	Range srvTextures;
	srvTextures.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, textureCount, 3); // t3?tN

	RootParam rootParams[ROOT_PARAM_COUNT] = {};

	rootParams[DRAW_INFO].InitAsConstants(3, 0);
	rootParams[CBV_VIEWPROJ].InitAsDescriptorTable(1, &cbvViewProj);
	rootParams[SRV_WORLD_MAT].InitAsDescriptorTable(1, &srvWorldMat);
	rootParams[SRV_BONES].InitAsDescriptorTable(1, &srvBones);
	rootParams[SRV_MATERIAL].InitAsDescriptorTable(1, &srvMaterial);
	rootParams[SRV_TEXTURES].InitAsDescriptorTable(1, &srvTextures); // テクスチャ数を可変に

	SamplerDesc samplers[SAMPLER_COUNT] = {};
	samplers[SAMPLER_LINEAR].Init(0);
	samplers[SAMPLER_ANISO].Init(1, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	samplers[SAMPLER_WRAP].Init(2, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	CD3DX12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.Init(
		ROOT_PARAM_COUNT,
		rootParams,
		SAMPLER_COUNT,
		samplers,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	ComPtr<ID3DBlob> rootSigBlob, errorBlob;

	HRESULT result = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);
	if (FAILED(result)) {
		if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		return;
	}

	ComPtr<ID3D12RootSignature> rootSignature;

	result = device->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		LOG_FATAL("Failed to create root signature");
		return;
	}
	m_impl->rootSignature = rootSignature;
}

DX12RootSignature::~DX12RootSignature()
{
	if (m_impl->rootSignature)
	{
		m_impl->rootSignature->Release();
		m_impl->rootSignature = nullptr;
	}
}