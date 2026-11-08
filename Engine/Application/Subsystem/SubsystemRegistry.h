#pragma once

#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Core/Log/Result.h"

#include <memory>
#include <span>
#include <string_view>
#include <vector>

class SubsystemContext;

// 責任: エンジンサブシステムの登録、初期化順、Tick 順、終了を管理する。
// 役割: 依存名から順を作り、EngineLoop がその順で初期化と Tick を呼ぶ。
// 所有: EngineLoop のメンバ。ISubsystem を unique_ptr で所有する。順序配列は登録済みインスタンスを指すだけで所有しない。
class SubsystemRegistry
{
public:
	void Register(std::unique_ptr<ISubsystem> subsystem);

	Result<void> BuildOrders();
	Result<void> InitializeAll(SubsystemContext& ctx);
	Result<void> PostInitializeAll(SubsystemContext& ctx);
	void ShutdownAll(SubsystemContext& ctx);

	[[nodiscard]] std::span<ISubsystem* const> GetInitOrder() const;
	[[nodiscard]] std::span<ISubsystem* const> GetTickOrder() const;

	// EngineLoop が起動中のレジストリを置く。終了で外す。所有はしない。
	static void SetCurrent(SubsystemRegistry* registry) noexcept;
	[[nodiscard]] static SubsystemRegistry* Current() noexcept;

	template<typename T>
	[[nodiscard]] T* Find() const
	{
		for (const std::unique_ptr<ISubsystem>& subsystem : m_subsystems)
		{
			if (T* typed = dynamic_cast<T*>(subsystem.get()))
			{
				return typed;
			}
		}

		return nullptr;
	}

	template<typename T>
	[[nodiscard]] static T* FindCurrent()
	{
		SubsystemRegistry* registry = Current();
		return registry != nullptr ? registry->Find<T>() : nullptr;
	}

private:
	[[nodiscard]] ISubsystem* FindByName(std::string_view name) const;

	std::vector<std::unique_ptr<ISubsystem>> m_subsystems{};
	std::vector<ISubsystem*> m_initOrder{};
	std::vector<ISubsystem*> m_tickOrder{};
	bool m_ordersBuilt = false;
};
