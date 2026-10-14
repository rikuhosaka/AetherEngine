#pragma once

#include "Engine/RHI/Interface/RHIImGui.h"

#include <memory>
#include <string>

class DX12Device;

class DX12ImGui final : public RHIImGui
{
public:
	~DX12ImGui() override;

	void NewFrame() override;
	void Render() override;
	void Draw(RHICommandList* commandList) override;
	void Shutdown() override;

	bool HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result) override;

	[[nodiscard]] bool WantCaptureMouse() const override;
	[[nodiscard]] bool WantCaptureKeyboard() const override;

	[[nodiscard]] bool IsValid() const;

private:
	struct Impl;

	DX12ImGui() = default;

	static Result<std::unique_ptr<DX12ImGui>> Create(const DX12Device* device, const RHIImGuiDesc& desc);

	[[nodiscard]] std::string Initialize(const DX12Device* device, const RHIImGuiDesc& desc);

	std::unique_ptr<Impl> m_impl{};

	friend class DX12Device;
};
