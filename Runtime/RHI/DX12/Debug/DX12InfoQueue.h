#pragma once

struct ID3D12Device;
struct IDXGIFactory6;

namespace DX12InfoQueue
{
#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG

	void AttachDevice(ID3D12Device* device);
	void DetachDevice();

	void AttachFactory(IDXGIFactory6* factory);
	void DetachFactory();

	void FlushPendingMessages();

#else

	inline void AttachDevice(ID3D12Device*) {}
	inline void DetachDevice() {}

	inline void AttachFactory(IDXGIFactory6*) {}
	inline void DetachFactory() {}
	inline void FlushPendingMessages() {}

#endif
}
