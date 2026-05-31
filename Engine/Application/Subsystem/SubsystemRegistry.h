#pragma once

#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Application/Subsystem/ISceneExtractor.h"
#include "Engine/Core/Log/Result.h"

#include <memory>
#include <span>
#include <string_view>
#include <vector>

class SubsystemContext;

class SubsystemRegistry
{
public:
	void Register(std::unique_ptr<ISubsystem> subsystem);

	Result<void> InitializeAll(SubsystemContext& ctx);
	Result<void> PostInitializeAll(SubsystemContext& ctx);
	void ShutdownAll(SubsystemContext& ctx);

	void SetSceneExtractor(ISceneExtractor* extractor);
	[[nodiscard]] ISceneExtractor* GetSceneExtractor() const;

	[[nodiscard]] std::span<ISubsystem* const> GetInitOrder() const;
	[[nodiscard]] std::span<ISubsystem* const> GetTickOrder() const;

private:
	Result<void> BuildOrders();

	[[nodiscard]] ISubsystem* FindByName(std::string_view name) const;

	std::vector<std::unique_ptr<ISubsystem>> m_subsystems{};
	std::vector<ISubsystem*> m_initOrder{};
	std::vector<ISubsystem*> m_tickOrder{};
	ISceneExtractor* m_sceneExtractor = nullptr;
	bool m_ordersBuilt = false;
};
