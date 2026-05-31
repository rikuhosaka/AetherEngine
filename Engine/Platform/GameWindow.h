#pragma once

#include "Engine/Core/Log/Result.h"

#include <Windows.h>

#include <cstdint>
#include <memory>

class GameWindow
{
public:
	struct CreateDesc
	{
		HINSTANCE hInstance = nullptr;
		uint32_t clientWidth = 1280;
		uint32_t clientHeight = 720;
		const wchar_t* title = L"DX12MyGameEngine";
		const wchar_t* className = L"DirectXEngine";
	};

	static Result<std::unique_ptr<GameWindow>> Create(const CreateDesc& desc);

	~GameWindow();

	GameWindow(const GameWindow&) = delete;
	GameWindow& operator=(const GameWindow&) = delete;

	[[nodiscard]] HWND GetHwnd() const noexcept { return m_hwnd; }
	[[nodiscard]] uint32_t GetClientWidth() const noexcept { return m_clientWidth; }
	[[nodiscard]] uint32_t GetClientHeight() const noexcept { return m_clientHeight; }
	[[nodiscard]] bool IsMinimized() const noexcept { return m_isMinimized; }

	[[nodiscard]] bool ProcessMessages();
	[[nodiscard]] bool HasPendingResize() const noexcept { return m_pendingResize; }
	void ClearPendingResize() noexcept { m_pendingResize = false; }
	[[nodiscard]] uint32_t GetPendingWidth() const noexcept { return m_pendingWidth; }
	[[nodiscard]] uint32_t GetPendingHeight() const noexcept { return m_pendingHeight; }

private:
	GameWindow() = default;

	Result<void> RegisterWindowClass(const CreateDesc& desc);
	Result<void> CreateNativeWindow(const CreateDesc& desc);
	void UpdateClientSizeFromWindow();
	void DestroyNativeWindow();

	static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	LRESULT HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

	HWND m_hwnd = nullptr;
	WNDCLASSEXW m_windowClass{};
	HINSTANCE m_hInstance = nullptr;
	uint32_t m_clientWidth = 0;
	uint32_t m_clientHeight = 0;
	bool m_isMinimized = false;
	bool m_classRegistered = false;
	bool m_pendingResize = false;
	uint32_t m_pendingWidth = 0;
	uint32_t m_pendingHeight = 0;
};
