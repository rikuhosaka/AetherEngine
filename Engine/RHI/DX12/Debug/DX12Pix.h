#pragma once

struct ID3D12GraphicsCommandList;

namespace DX12Pix
{
	// Load WinPixEventRuntime.dll / optional PIX GPU capturer. Safe to call without PIX build.
	void Initialize();

	bool IsEnabled();

	void BeginEvent(ID3D12GraphicsCommandList* commandList, const char* name);
	void EndEvent(ID3D12GraphicsCommandList* commandList);

} // namespace DX12Pix
