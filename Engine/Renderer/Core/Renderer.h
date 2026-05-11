#pragma once
#include <Engine/Renderer/RenderItem/RenderItem.h>


class Renderer
{
public:
	Renderer();
	~Renderer();
    void Initialize();
    void BeginFrame();
    void EndFrame();

    void Submit(RenderItem* item);
    void Render();
};