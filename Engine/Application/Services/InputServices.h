#pragma once

class InputState;

struct InputServices
{
	const InputState* state = nullptr;
	bool relativeMouse = false;
};
