#pragma once

#include <cstdint>
#include <typeindex>
#include <unordered_map>

class SubsystemContext
{
public:
	template<typename T>
	void RegisterService(T* service)
	{
		m_services[std::type_index(typeid(T))] = service;
	}

	template<typename T>
	[[nodiscard]] T* GetService() const
	{
		const auto it = m_services.find(std::type_index(typeid(T)));
		return it != m_services.end() ? static_cast<T*>(it->second) : nullptr;
	}

	void SetDeltaSeconds(float deltaSeconds) { m_deltaSeconds = deltaSeconds; }
	[[nodiscard]] float GetDeltaSeconds() const { return m_deltaSeconds; }

	void SetFrameSlot(uint32_t frameSlot) { m_frameSlot = frameSlot; }
	[[nodiscard]] uint32_t GetFrameSlot() const { return m_frameSlot; }

private:
	std::unordered_map<std::type_index, void*> m_services{};
	float m_deltaSeconds = 0.0f;
	uint32_t m_frameSlot = 0;
};
