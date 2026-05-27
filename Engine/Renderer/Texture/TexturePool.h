#pragma once

#include "Engine/Core/Containers/ResourcePool.h"
#include "Engine/Renderer/Texture/TextureTypes.h"

class TexturePool
{
public:
	[[nodiscard]] TextureHandle Add(std::unique_ptr<Texture> texture);
	[[nodiscard]] Texture* Get(TextureHandle handle);
	void Remove(TextureHandle handle);

private:
	ResourcePool<Texture> m_pool{};
};
