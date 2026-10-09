#include "Engine/Platform/Win32InputDevice.h"

namespace
{
struct KeyBinding
{
	int virtualKey;
	Key key;
};

constexpr KeyBinding kKeyboardBindings[] = {
	{ 'A', Key::A }, { 'B', Key::B }, { 'C', Key::C }, { 'D', Key::D },
	{ 'E', Key::E }, { 'F', Key::F }, { 'G', Key::G }, { 'H', Key::H },
	{ 'I', Key::I }, { 'J', Key::J }, { 'K', Key::K }, { 'L', Key::L },
	{ 'M', Key::M }, { 'N', Key::N }, { 'O', Key::O }, { 'P', Key::P },
	{ 'Q', Key::Q }, { 'R', Key::R }, { 'S', Key::S }, { 'T', Key::T },
	{ 'U', Key::U }, { 'V', Key::V }, { 'W', Key::W }, { 'X', Key::X },
	{ 'Y', Key::Y }, { 'Z', Key::Z },
	{ '0', Key::Digit0 }, { '1', Key::Digit1 }, { '2', Key::Digit2 },
	{ '3', Key::Digit3 }, { '4', Key::Digit4 }, { '5', Key::Digit5 },
	{ '6', Key::Digit6 }, { '7', Key::Digit7 }, { '8', Key::Digit8 },
	{ '9', Key::Digit9 },
	{ VK_SPACE, Key::Space },
	{ VK_ESCAPE, Key::Escape },
	{ VK_LEFT, Key::Left },
	{ VK_RIGHT, Key::Right },
	{ VK_UP, Key::Up },
	{ VK_DOWN, Key::Down },
};

constexpr KeyBinding kMouseBindings[] = {
	{ VK_LBUTTON, Key::MouseLeft },
	{ VK_RBUTTON, Key::MouseRight },
	{ VK_MBUTTON, Key::MouseMiddle },
	{ VK_XBUTTON1, Key::MouseX1 },
	{ VK_XBUTTON2, Key::MouseX2 },
};

constexpr std::size_t KeyCount = static_cast<std::size_t>(Key::Count);

std::size_t IndexOf(Key key)
{
	return static_cast<std::size_t>(key);
}

bool IsVirtualKeyDown(int virtualKey)
{
	return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool IsForegroundWindow(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight)
{
	return hwnd != nullptr
		&& clientWidth > 0
		&& clientHeight > 0
		&& IsIconic(hwnd) == FALSE
		&& GetForegroundWindow() == hwnd;
}

MousePixelPosition ReadCursorClientPosition(HWND hwnd)
{
	POINT point{};
	GetCursorPos(&point);
	ScreenToClient(hwnd, &point);
	return { point.x, point.y };
}

MousePixelPosition ClientCenter(std::uint32_t clientWidth, std::uint32_t clientHeight)
{
	return {
		static_cast<int>(clientWidth / 2),
		static_cast<int>(clientHeight / 2),
	};
}

bool TryGetClientClipRect(HWND hwnd, RECT& outRect)
{
	RECT client{};
	if (GetClientRect(hwnd, &client) == FALSE)
	{
		return false;
	}

	POINT topLeft{ client.left, client.top };
	POINT bottomRight{ client.right, client.bottom };
	if (ClientToScreen(hwnd, &topLeft) == FALSE || ClientToScreen(hwnd, &bottomRight) == FALSE)
	{
		return false;
	}

	outRect.left = topLeft.x;
	outRect.top = topLeft.y;
	outRect.right = bottomRight.x;
	outRect.bottom = bottomRight.y;
	return outRect.right > outRect.left && outRect.bottom > outRect.top;
}

void ClipCursorToClient(HWND hwnd)
{
	RECT clip{};
	if (TryGetClientClipRect(hwnd, clip))
	{
		ClipCursor(&clip);
	}
}

void MoveCursorToClientPoint(HWND hwnd, MousePixelPosition clientPosition)
{
	POINT screen{ clientPosition.x, clientPosition.y };
	if (ClientToScreen(hwnd, &screen) != FALSE)
	{
		SetCursorPos(screen.x, screen.y);
	}
}
} // namespace

Win32InputDevice::~Win32InputDevice()
{
	if (!m_relativeActive && !m_cursorHidden)
	{
		return;
	}

	ClipCursor(nullptr);
	ReleaseCapture();
	RestoreCursor();
	if (m_relativeActive)
	{
		SetCursorPos(m_savedCursorScreen.x, m_savedCursorScreen.y);
	}
	m_relativeActive = false;
}

void Win32InputDevice::Reset(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state)
{
	m_wasDown.fill(false);
	state.m_buttons.fill({});
	state.m_mousePixelPos = {};
	state.m_mouseDelta = {};
	m_lastMousePixelPos = {};

	if (hwnd == nullptr || clientWidth == 0 || clientHeight == 0)
	{
		return;
	}

	m_lastMousePixelPos = ReadCursorClientPosition(hwnd);
	state.m_mousePixelPos = m_lastMousePixelPos;
}

void Win32InputDevice::Update(
	HWND hwnd,
	std::uint32_t clientWidth,
	std::uint32_t clientHeight,
	InputState& state,
	bool relativeMouseRequested)
{
	const bool focused = IsForegroundWindow(hwnd, clientWidth, clientHeight);
	if (!focused && m_relativeActive)
	{
		EndRelativeMouse(hwnd, state);
	}

	WriteButtons(focused, state);

	if (!focused)
	{
		state.m_mouseDelta = {};
		return;
	}

	if (relativeMouseRequested && !m_relativeActive)
	{
		BeginRelativeMouse(hwnd, clientWidth, clientHeight, state);
		return;
	}

	if (!relativeMouseRequested && m_relativeActive)
	{
		EndRelativeMouse(hwnd, state);
		return;
	}

	if (m_relativeActive)
	{
		ApplyRelativeMouse(hwnd, clientWidth, clientHeight, state);
		return;
	}

	WriteMouse(hwnd, clientWidth, clientHeight, state);
}

void Win32InputDevice::RefreshRelativeMouse(
	HWND hwnd,
	std::uint32_t clientWidth,
	std::uint32_t clientHeight,
	InputState& state)
{
	if (!m_relativeActive || hwnd == nullptr || clientWidth == 0 || clientHeight == 0)
	{
		return;
	}

	ClipCursorToClient(hwnd);
	const MousePixelPosition center = ClientCenter(clientWidth, clientHeight);
	MoveCursorToClientPoint(hwnd, center);
	m_lastMousePixelPos = center;
	state.m_mousePixelPos = center;
	state.m_mouseDelta = {};
}

void Win32InputDevice::WriteButtons(bool focused, InputState& state)
{
	std::array<bool, KeyCount> down{};
	if (focused)
	{
		for (const KeyBinding& binding : kKeyboardBindings)
		{
			down[IndexOf(binding.key)] = IsVirtualKeyDown(binding.virtualKey);
		}

		const bool shift = IsVirtualKeyDown(VK_SHIFT)
			|| IsVirtualKeyDown(VK_LSHIFT)
			|| IsVirtualKeyDown(VK_RSHIFT);
		const bool control = IsVirtualKeyDown(VK_CONTROL)
			|| IsVirtualKeyDown(VK_LCONTROL)
			|| IsVirtualKeyDown(VK_RCONTROL);
		down[IndexOf(Key::LeftShift)] = shift;
		down[IndexOf(Key::LeftControl)] = control;

		for (const KeyBinding& binding : kMouseBindings)
		{
			down[IndexOf(binding.key)] = IsVirtualKeyDown(binding.virtualKey);
		}
	}

	for (std::size_t index = 0; index < KeyCount; ++index)
	{
		const bool isDown = down[index];
		InputState::Button& button = state.m_buttons[index];
		button.down = isDown;
		button.pressed = isDown && !m_wasDown[index];
		button.released = !isDown && m_wasDown[index];
		m_wasDown[index] = isDown;
	}
}

void Win32InputDevice::WriteMouse(
	HWND hwnd,
	std::uint32_t clientWidth,
	std::uint32_t clientHeight,
	InputState& state)
{
	if (hwnd == nullptr || clientWidth == 0 || clientHeight == 0)
	{
		state.m_mouseDelta = {};
		return;
	}

	const MousePixelPosition position = ReadCursorClientPosition(hwnd);
	state.m_mouseDelta = {
		position.x - m_lastMousePixelPos.x,
		position.y - m_lastMousePixelPos.y,
	};
	state.m_mousePixelPos = position;
	m_lastMousePixelPos = position;
}

void Win32InputDevice::BeginRelativeMouse(
	HWND hwnd,
	std::uint32_t clientWidth,
	std::uint32_t clientHeight,
	InputState& state)
{
	GetCursorPos(&m_savedCursorScreen);
	HideCursor();
	ClipCursorToClient(hwnd);
	SetCapture(hwnd);

	const MousePixelPosition center = ClientCenter(clientWidth, clientHeight);
	MoveCursorToClientPoint(hwnd, center);
	m_lastMousePixelPos = center;
	state.m_mousePixelPos = center;
	state.m_mouseDelta = {};
	m_relativeActive = true;
}

void Win32InputDevice::EndRelativeMouse(HWND hwnd, InputState& state)
{
	if (!m_relativeActive)
	{
		return;
	}

	ClipCursor(nullptr);
	ReleaseCapture();
	RestoreCursor();
	SetCursorPos(m_savedCursorScreen.x, m_savedCursorScreen.y);
	m_relativeActive = false;

	state.m_mouseDelta = {};
	if (hwnd != nullptr)
	{
		m_lastMousePixelPos = ReadCursorClientPosition(hwnd);
		state.m_mousePixelPos = m_lastMousePixelPos;
	}
}

void Win32InputDevice::ApplyRelativeMouse(
	HWND hwnd,
	std::uint32_t clientWidth,
	std::uint32_t clientHeight,
	InputState& state)
{
	const MousePixelPosition center = ClientCenter(clientWidth, clientHeight);
	const MousePixelPosition position = ReadCursorClientPosition(hwnd);
	state.m_mouseDelta = {
		position.x - center.x,
		position.y - center.y,
	};
	state.m_mousePixelPos = center;
	m_lastMousePixelPos = center;
	MoveCursorToClientPoint(hwnd, center);
}

void Win32InputDevice::HideCursor()
{
	if (m_cursorHidden)
	{
		return;
	}

	ShowCursor(FALSE);
	m_cursorHidden = true;
}

void Win32InputDevice::RestoreCursor()
{
	if (!m_cursorHidden)
	{
		return;
	}

	ShowCursor(TRUE);
	m_cursorHidden = false;
}
