#pragma once

#include "Engine/Core/Log/Result.h"

#include <memory>

class RHICommandList;
class RHICommandQueue;

struct RHIImGuiDesc
{
	HWND hwnd = nullptr;
	RHICommandQueue* commandQueue = nullptr;
	uint32_t framesInFlight = 3;
};

// Destroy this before the device and command queue it was created from.
class RHIImGui
{
public:
	virtual ~RHIImGui() = default;

	virtual void NewFrame() = 0;
	virtual void Render() = 0;
	virtual void Draw(RHICommandList* commandList) = 0;
	virtual void Shutdown() = 0;

	// Returns true when the message is consumed and result should be returned from the window procedure.
	virtual bool HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, LRESULT& result) = 0;

	[[nodiscard]] virtual bool WantCaptureMouse() const = 0;
	[[nodiscard]] virtual bool WantCaptureKeyboard() const = 0;
};
