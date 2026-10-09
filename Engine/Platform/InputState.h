#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct MousePixelPosition
{
	int x = 0;
	int y = 0;
};

enum class Key : std::uint8_t
{
	A, B, C, D, E, F, G, H, I, J, K, L, M,
	N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	Digit0, Digit1, Digit2, Digit3, Digit4,
	Digit5, Digit6, Digit7, Digit8, Digit9,
	Space,
	LeftShift,
	LeftControl,
	Escape,
	Left,
	Right,
	Up,
	Down,
	MouseLeft,
	MouseRight,
	MouseMiddle,
	MouseX1,
	MouseX2,
	Count
};

class Win32InputDevice;

class InputState
{
public:
	[[nodiscard]] bool IsDown(Key key) const;
	[[nodiscard]] bool WasPressed(Key key) const;
	[[nodiscard]] bool WasReleased(Key key) const;

	[[nodiscard]] MousePixelPosition GetMousePositionPixels() const noexcept { return m_mousePixelPos; }
	[[nodiscard]] MousePixelPosition GetMouseDeltaPixels() const noexcept { return m_mouseDelta; }

private:
	friend class Win32InputDevice;

	struct Button
	{
		bool down = false;
		bool pressed = false;
		bool released = false;
	};

	[[nodiscard]] const Button* Find(Key key) const noexcept;

	std::array<Button, static_cast<std::size_t>(Key::Count)> m_buttons{};
	MousePixelPosition m_mousePixelPos{};
	MousePixelPosition m_mouseDelta{};
};

inline const InputState::Button* InputState::Find(Key key) const noexcept
{
	const auto index = static_cast<std::size_t>(key);
	if (index >= m_buttons.size())
	{
		return nullptr;
	}

	return &m_buttons[index];
}

inline bool InputState::IsDown(Key key) const
{
	const Button* button = Find(key);
	return button != nullptr && button->down;
}

inline bool InputState::WasPressed(Key key) const
{
	const Button* button = Find(key);
	return button != nullptr && button->pressed;
}

inline bool InputState::WasReleased(Key key) const
{
	const Button* button = Find(key);
	return button != nullptr && button->released;
}
