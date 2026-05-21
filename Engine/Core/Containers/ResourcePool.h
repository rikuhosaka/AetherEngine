#pragma once
#include "Engine/Core/Handle/Handle.h"


template<typename T>
class ResourcePool
{
public:

    Handle<T> Add(std::unique_ptr<T> resource)
    {
        uint32_t index;

        if (!m_freeIndices.empty())
        {
            index = m_freeIndices.back();
            m_freeIndices.pop_back();

            m_resources[index] = std::move(resource);
        }
        else
        {
            index = static_cast<uint32_t>(m_resources.size());

            m_resources.push_back(std::move(resource));
            m_generations.push_back(1);
        }

        return { index, m_generations[index] };
    }

    T* Get(Handle<T> handle)
    {
        if (handle.Index >= m_resources.size())
            return nullptr;

        if (m_generations[handle.Index] != handle.Generation)
            return nullptr;

        return m_resources[handle.Index].get();
    }

    void Remove(Handle<T> handle)
    {
        if (Get(handle) == nullptr)
            return;

        m_resources[handle.Index].reset();

        m_generations[handle.Index]++;

        m_freeIndices.push_back(handle.Index);
    }

private:

    std::vector<std::unique_ptr<T>> m_resources;
    std::vector<uint32_t> m_generations;
    std::vector<uint32_t> m_freeIndices;
};