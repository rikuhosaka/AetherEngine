#pragma once

class DeviceImpl
{
public:
	ComPtr<ID3D12Device> device;
	ComPtr<IDXGIFactory6> factory;
};