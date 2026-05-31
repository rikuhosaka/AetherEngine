#include "Engine/Platform/GameWindow.h"

#include <string>

namespace
{
constexpr wchar_t kWindowClassName[] = L"DirectXEngine";
}

Result<std::unique_ptr<GameWindow>> GameWindow::Create(const CreateDesc& desc)
{
	if (desc.hInstance == nullptr)
	{
		return MakeFail<std::unique_ptr<GameWindow>>(
			ErrorCode::InvalidArgument,
			"GameWindow::Create requires a valid HINSTANCE");
	}

	if (desc.clientWidth == 0 || desc.clientHeight == 0)
	{
		return MakeFail<std::unique_ptr<GameWindow>>(
			ErrorCode::InvalidArgument,
			"GameWindow::Create requires non-zero client dimensions");
	}

	auto window = std::unique_ptr<GameWindow>(new GameWindow());
	window->m_hInstance = desc.hInstance;

	if (auto registerResult = window->RegisterWindowClass(desc); !registerResult)
	{
		return MakeFail<std::unique_ptr<GameWindow>>(
			registerResult.error.code,
			registerResult.error.message);
	}

	if (auto createResult = window->CreateNativeWindow(desc); !createResult)
	{
		return MakeFail<std::unique_ptr<GameWindow>>(
			createResult.error.code,
			createResult.error.message);
	}

	window->UpdateClientSizeFromWindow();
	return MakeOk(std::move(window));
}

GameWindow::~GameWindow()
{
	DestroyNativeWindow();
}

Result<void> GameWindow::RegisterWindowClass(const CreateDesc& desc)
{
	const wchar_t* className = desc.className != nullptr ? desc.className : kWindowClassName;

	m_windowClass = {};
	m_windowClass.cbSize = sizeof(WNDCLASSEXW);
	m_windowClass.style = CS_HREDRAW | CS_VREDRAW;
	m_windowClass.lpfnWndProc = GameWindow::WindowProcedure;
	m_windowClass.hInstance = desc.hInstance;
	m_windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
	m_windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	m_windowClass.lpszClassName = className;

	if (RegisterClassExW(&m_windowClass) == 0)
	{
		return MakeFail(ErrorCode::ResourceCreationFailed, "Failed to register window class");
	}

	m_classRegistered = true;
	return MakeOk();
}

Result<void> GameWindow::CreateNativeWindow(const CreateDesc& desc)
{
	const wchar_t* className = desc.className != nullptr ? desc.className : kWindowClassName;
	const wchar_t* title = desc.title != nullptr ? desc.title : L"DX12MyGameEngine";

	RECT windowRect = {
		0,
		0,
		static_cast<LONG>(desc.clientWidth),
		static_cast<LONG>(desc.clientHeight),
	};
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	m_hwnd = CreateWindowExW(
		0,
		className,
		title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		desc.hInstance,
		this);

	if (m_hwnd == nullptr)
	{
		return MakeFail(ErrorCode::ResourceCreationFailed, "Failed to create window");
	}

	SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	ShowWindow(m_hwnd, SW_SHOW);
	UpdateWindow(m_hwnd);
	return MakeOk();
}

void GameWindow::UpdateClientSizeFromWindow()
{
	if (m_hwnd == nullptr)
	{
		return;
	}

	RECT clientRect{};
	GetClientRect(m_hwnd, &clientRect);
	m_clientWidth = static_cast<uint32_t>(clientRect.right - clientRect.left);
	m_clientHeight = static_cast<uint32_t>(clientRect.bottom - clientRect.top);
	m_isMinimized = IsIconic(m_hwnd) != FALSE;
}

void GameWindow::DestroyNativeWindow()
{
	if (m_hwnd != nullptr)
	{
		SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
		DestroyWindow(m_hwnd);
		m_hwnd = nullptr;
	}

	if (m_classRegistered)
	{
		const wchar_t* className = m_windowClass.lpszClassName != nullptr
			? m_windowClass.lpszClassName
			: kWindowClassName;
		UnregisterClassW(className, m_hInstance);
		m_classRegistered = false;
	}
}

bool GameWindow::ProcessMessages()
{
	MSG msg{};
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return true;
}

LRESULT CALLBACK GameWindow::WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_NCCREATE)
	{
		const CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lparam);
		auto* window = static_cast<GameWindow*>(createStruct->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		return TRUE;
	}

	auto* window = reinterpret_cast<GameWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (window != nullptr)
	{
		return window->HandleMessage(msg, wparam, lparam);
	}

	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT GameWindow::HandleMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_SIZE:
	{
		m_clientWidth = LOWORD(lparam);
		m_clientHeight = HIWORD(lparam);
		m_isMinimized = wparam == SIZE_MINIMIZED;

		if (wparam != SIZE_MINIMIZED)
		{
			m_pendingResize = true;
			m_pendingWidth = m_clientWidth;
			m_pendingHeight = m_clientHeight;
		}

		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProcW(m_hwnd, msg, wparam, lparam);
	}
}
