#pragma once


class FenceImpl
{
public:
	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent;
	uint64_t currentFenceValue;
};