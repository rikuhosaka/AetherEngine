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

MousePixelPosition ReadCursorClientPosition(HWND hwnd)
{
	POINT point{};
	GetCursorPos(&point);
	ScreenToClient(hwnd, &point);
	return { point.x, point.y };
}
} // namespace

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

void Win32InputDevice::Update(HWND hwnd, std::uint32_t clientWidth, std::uint32_t clientHeight, InputState& state)
{
	std::array<bool, KeyCount> down{};

	BYTE keyState[256] = {};
	if (GetKeyboardState(keyState))
	{
		for (const KeyBinding& binding : kKeyboardBindings)
		{
			down[IndexOf(binding.key)] = (keyState[binding.virtualKey] & 0x80) != 0;
		}

		// Either physical side sets the combined virtual key.
		const bool shift = (keyState[VK_SHIFT] & 0x80) != 0
			|| (keyState[VK_LSHIFT] & 0x80) != 0
			|| (keyState[VK_RSHIFT] & 0x80) != 0;
		const bool control = (keyState[VK_CONTROL] & 0x80) != 0
			|| (keyState[VK_LCONTROL] & 0x80) != 0
			|| (keyState[VK_RCONTROL] & 0x80) != 0;
		down[IndexOf(Key::LeftShift)] = shift;
		down[IndexOf(Key::LeftControl)] = control;
	}

	for (const KeyBinding& binding : kMouseBindings)
	{
		down[IndexOf(binding.key)] = (GetAsyncKeyState(binding.virtualKey) & 0x8000) != 0;
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

	WriteMouse(hwnd, clientWidth, clientHeight, state);
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
