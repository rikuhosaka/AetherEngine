#pragma once
#include <string>
#include <memory>
#include <cstring>
#include <Windows.h>
#include <map>

struct MousePosition {
    float x;
    float y;
};

class InputManager
{
public:
    

    static InputManager& Get()
    {
        static InputManager instance;
        return instance;
    }

    void Init();

    // --- ÉLÅ[èÛë‘éÊìæ ---
    bool IsKeyPressed(int key) const { return keys[key]; }
    bool IsKeyDown(int key) const { return keys[key] && !lastKeys[key]; }
    bool IsKeyUp(int key) const { return !keys[key] && lastKeys[key]; }

	MousePosition GetMousePositionNDC() const { return mousePos; }
    MousePosition GetMousePositionDelta() const { return { mousePos.x - lastMousePos.x, mousePos.y - lastMousePos.y };}

    void Update();
private:
    InputManager() = default;
    InputManager(const InputManager&) = delete;
    void operator = (const InputManager&) = delete;

	~InputManager() = default;

    bool keys[256] = {};
    bool lastKeys[256] = {};
    byte keyState[256] = {};

    MousePosition mousePos{};
    MousePosition lastMousePos{};

};
