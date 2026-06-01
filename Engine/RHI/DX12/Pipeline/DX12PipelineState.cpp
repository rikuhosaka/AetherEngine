#include "DX12PipelineState.h"

#include "Engine/RHI/DX12/Common/DX12Result.h"
#include "Engine/RHI/DX12/Device/DX12Device.h"
#include "Engine/RHI/DX12/Device/DeviceImpl.h"
#include "Engine/RHI/DX12/Resource/DX12VertexShader.h"
#include "Engine/RHI/DX12/Resource/DX12PixelShader.h"
#include "Engine/RHI/DX12/Resource/ShaderImpl.h"
#include "Engine/RHI/DX12/Pipeline/PipelineImpl.h"
#include "Engine/RHI/DX12/Pipeline/DX12RootSignature.h"
#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"


PipelineStateImpl*
DX12PipelineState::GetImpl() const
{
	return m_impl.get();
}

bool DX12PipelineState::IsValid() const
{
	return m_impl != nullptr && m_impl->pipelineState != nullptr;
}

Result<std::unique_ptr<DX12PipelineState>> DX12PipelineState::Create(
	const RHIPipelineDesc& pipelineDesc,
	const DX12Device* dxDevice)
{
	return MakeResourceResult(
		std::unique_ptr<DX12PipelineState>(new DX12PipelineState(pipelineDesc, dxDevice)),
		"Failed to create pipeline state");
}

DX12PipelineState::DX12PipelineState(const RHIPipelineDesc& pipelineDesc, const DX12Device* dxDevice)
	: m_impl(std::make_unique<PipelineStateImpl>())
{
	ID3D12Device* device = dxDevice->GetImpl()->device.Get();
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	//?V?F?[?_?[
	auto vs = static_cast<DX12VertexShader*>(pipelineDesc.vertexShader);
	auto ps = static_cast<DX12PixelShader*>(pipelineDesc.pixelShader);
	gpipeline.VS = { vs->GetImpl()->blob->GetBufferPointer(), vs->GetImpl()->blob->GetBufferSize() };
	gpipeline.PS = { ps->GetImpl()->blob->GetBufferPointer(), ps->GetImpl()->blob->GetBufferSize() };

	if (pipelineDesc.rootSignature != nullptr)
	{
		const auto* dxRootSignature = static_cast<const DX12RootSignature*>(pipelineDesc.rootSignature);
		gpipeline.pRootSignature = dxRootSignature->GetImpl()->rootSignature.Get();
	}

	if (pipelineDesc.inputLayout == InputLayoutType::PositionTex)
	{
		D3D12_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		gpipeline.InputLayout.pInputElementDescs = layout;
		gpipeline.InputLayout.NumElements = _countof(layout);
	}
	else if (pipelineDesc.inputLayout == InputLayoutType::Basic)
	{
		D3D12_INPUT_ELEMENT_DESC basicLayout[] = {
		{ "POSITION",0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 },
		{ "NORMAL",0, DXGI_FORMAT_R32G32B32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0 }
		};
		gpipeline.InputLayout.pInputElementDescs = basicLayout;
		gpipeline.InputLayout.NumElements = _countof(basicLayout);
	}
	else if (pipelineDesc.inputLayout == InputLayoutType::Skinned)
	{
		D3D12_INPUT_ELEMENT_DESC fbxLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		gpipeline.InputLayout.pInputElementDescs = fbxLayout;
		gpipeline.InputLayout.NumElements = _countof(fbxLayout);
	}

	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.SampleDesc = { 1, 0 };
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.NodeMask = 0;
	gpipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ComPtr<ID3D12PipelineState> pipelineState;
	HRESULT result = device->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&pipelineState));
	if (FAILED(result))
	{
		return;
	}
	m_impl->pipelineState = pipelineState;
	DX12GpuNaming::SetResourceName(pipelineState.Get(), "PSO", pipelineDesc.DebugName);
}

DX12PipelineState::~DX12PipelineState()
{
	if (m_impl->pipelineState)
	{
		m_impl->pipelineState->Release();
		m_impl->pipelineState = nullptr;
	}
}