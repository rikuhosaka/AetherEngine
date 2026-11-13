#pragma once


class PipelineStateImpl
{
public:
	ComPtr<ID3D12PipelineState> pipelineState;
};

class RootSignatureImpl
{
public:
	ComPtr<ID3D12RootSignature> rootSignature;
};