#include "Engine/Application/Subsystem/SubsystemRegistry.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"

#include <algorithm>
#include <queue>
#include <string>
#include <unordered_map>

namespace
{
bool CompareTickOrder(ISubsystem* lhs, ISubsystem* rhs)
{
	return static_cast<int32_t>(lhs->GetTickGroup()) < static_cast<int32_t>(rhs->GetTickGroup());
}
} // namespace

void SubsystemRegistry::Register(std::unique_ptr<ISubsystem> subsystem)
{
	if (subsystem == nullptr)
	{
		return;
	}

	m_subsystems.push_back(std::move(subsystem));
	m_ordersBuilt = false;
}

ISubsystem* SubsystemRegistry::FindByName(std::string_view name) const
{
	for (const std::unique_ptr<ISubsystem>& subsystem : m_subsystems)
	{
		if (subsystem != nullptr && name == subsystem->GetName())
		{
			return subsystem.get();
		}
	}

	return nullptr;
}

Result<void> SubsystemRegistry::BuildOrders()
{
	if (m_ordersBuilt)
	{
		return MakeOk();
	}

	m_initOrder.clear();
	m_tickOrder.clear();

	std::unordered_map<std::string_view, ISubsystem*> subsystemByName{};
	subsystemByName.reserve(m_subsystems.size());

	for (const std::unique_ptr<ISubsystem>& subsystem : m_subsystems)
	{
		if (subsystem == nullptr)
		{
			continue;
		}

		const std::string_view name = subsystem->GetName();
		if (name.empty())
		{
			return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
				"Subsystem name must not be empty");
		}

		if (subsystemByName.contains(name))
		{
			return FailInternal(
				LogCategory::Core,
				ErrorCode::InvalidArgument,
				std::string("Duplicate subsystem name: ") + std::string(name));
		}

		subsystemByName.emplace(name, subsystem.get());
	}

	std::unordered_map<std::string_view, uint32_t> inDegree{};
	std::unordered_map<std::string_view, std::vector<std::string_view>> dependents{};

	for (const auto& [name, subsystem] : subsystemByName)
	{
		inDegree[name] = 0;

		for (const char* const dependencyName : subsystem->GetDependencies())
		{
			if (dependencyName == nullptr || dependencyName[0] == '\0')
			{
				continue;
			}

			const std::string_view dependencyView = dependencyName;
			if (!subsystemByName.contains(dependencyView))
			{
				return FailInternal(
					LogCategory::Core,
					ErrorCode::InvalidArgument,
					std::string("Subsystem '")
						+ std::string(name)
						+ "' depends on unknown subsystem '"
						+ std::string(dependencyView)
						+ "'");
			}

			++inDegree[name];
			dependents[dependencyView].push_back(name);
		}
	}

	std::queue<std::string_view> ready{};
	for (const auto& [name, degree] : inDegree)
	{
		if (degree == 0)
		{
			ready.push(name);
		}
	}

	m_initOrder.reserve(subsystemByName.size());
	while (!ready.empty())
	{
		const std::string_view name = ready.front();
		ready.pop();

		ISubsystem* subsystem = subsystemByName[name];
		m_initOrder.push_back(subsystem);

		for (const std::string_view dependentName : dependents[name])
		{
			uint32_t& degree = inDegree[dependentName];
			--degree;
			if (degree == 0)
			{
				ready.push(dependentName);
			}
		}
	}

	if (m_initOrder.size() != subsystemByName.size())
	{
		return FailInternal(LogCategory::Core, ErrorCode::InvalidArgument,
			"Circular subsystem dependency detected");
	}

	for (ISubsystem* subsystem : m_initOrder)
	{
		if (subsystem != nullptr && subsystem->WantsTick())
		{
			m_tickOrder.push_back(subsystem);
		}
	}

	std::stable_sort(m_tickOrder.begin(), m_tickOrder.end(), CompareTickOrder);

	m_ordersBuilt = true;
	return MakeOk();
}

Result<void> SubsystemRegistry::InitializeAll(SubsystemContext& ctx)
{
	if (auto buildResult = BuildOrders(); !buildResult)
	{
		return buildResult;
	}

	for (ISubsystem* subsystem : m_initOrder)
	{
		if (auto initResult = subsystem->Initialize(ctx); !initResult)
		{
			LOG_ERROR(LogCategory::Core, std::string("Subsystem '") + std::string(subsystem->GetName()) + "' failed to initialize");
			LogResult(initResult, LogCategory::Core);
			return initResult;
		}
	}

	return MakeOk();
}

Result<void> SubsystemRegistry::PostInitializeAll(SubsystemContext& ctx)
{
	if (auto buildResult = BuildOrders(); !buildResult)
	{
		return buildResult;
	}

	for (ISubsystem* subsystem : m_initOrder)
	{
		if (auto postInitResult = subsystem->PostInitialize(ctx); !postInitResult)
		{
			LogResult(postInitResult, LogCategory::Core);
			return postInitResult;
		}
	}

	return MakeOk();
}

void SubsystemRegistry::ShutdownAll(SubsystemContext& ctx)
{
	if (!m_ordersBuilt)
	{
		if (auto buildResult = BuildOrders(); !buildResult)
		{
			LogFatalResult(buildResult, LogCategory::Core);
			return;
		}
	}

	for (auto it = m_initOrder.rbegin(); it != m_initOrder.rend(); ++it)
	{
		if (*it != nullptr)
		{
			(*it)->Shutdown(ctx);
		}
	}
}

void SubsystemRegistry::SetSceneExtractor(ISceneExtractor* extractor)
{
	m_sceneExtractor = extractor;
}

ISceneExtractor* SubsystemRegistry::GetSceneExtractor() const
{
	return m_sceneExtractor;
}

std::span<ISubsystem* const> SubsystemRegistry::GetInitOrder() const
{
	return m_initOrder;
}

std::span<ISubsystem* const> SubsystemRegistry::GetTickOrder() const
{
	return m_tickOrder;
}
