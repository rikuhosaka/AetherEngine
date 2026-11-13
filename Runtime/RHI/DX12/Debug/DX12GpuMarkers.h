#pragma once

struct ID3D12GraphicsCommandList;

namespace DX12GpuMarkers
{
	void BeginEvent(ID3D12GraphicsCommandList* commandList, const char* name);
	void EndEvent(ID3D12GraphicsCommandList* commandList);

} // namespace DX12GpuMarkers
