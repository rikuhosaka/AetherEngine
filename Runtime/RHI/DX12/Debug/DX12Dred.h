#pragma once

#include <string_view>

struct ID3D12Device;

namespace DX12Dred
{
	void ConfigureDevice(ID3D12Device* device);

	[[nodiscard]] bool IsDeviceRemovedHresult(HRESULT hr);

	void ReportDeviceRemoved(ID3D12Device* device, std::string_view context);

	void CheckHresult(ID3D12Device* device, HRESULT hr, std::string_view context);

	// Call after CPU/GPU sync points (e.g. fence wait) to detect silent device loss.
	void CheckDeviceHealth(ID3D12Device* device, std::string_view context);

} // namespace DX12Dred
