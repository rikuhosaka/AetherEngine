#include "Engine/Renderer/Texture/TexturePool.h"

TextureHandle TexturePool::Add(std::unique_ptr<Texture> texture)
{
	return m_pool.Add(std::move(texture));
}

Texture* TexturePool::Get(TextureHandle handle)
{
	return m_pool.Get(handle);
}

void TexturePool::Remove(TextureHandle handle)
{
	m_pool.Remove(handle);
}
