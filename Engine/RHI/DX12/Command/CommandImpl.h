#pragma once


class CommandListImpl
{
public:
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ComPtr<ID3D12CommandAllocator> commandAllocator;
};


class CommandQueueImpl
{
public:
	ComPtr<ID3D12CommandQueue> commandQueue;
};