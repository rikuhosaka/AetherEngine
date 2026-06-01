#pragma once


class FenceImpl
{
public:
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent = nullptr;
	uint64_t currentFenceValue = 0;
};