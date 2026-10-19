#include "Engine/Application/Subsystem/SubsystemRegistry.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"

#include <algorithm>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

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

	struct Node
	{
		ISubsystem* subsystem = nullptr;
		uint32_t inDegree = 0;
		std::vector<uint32_t> dependents{};
	};

	std::vector<Node> nodes{};
	nodes.reserve(m_subsystems.size());
	std::unordered_map<std::string_view, uint32_t> indexByName{};
	indexByName.reserve(m_subsystems.size());

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

		if (indexByName.contains(name))
		{
			return FailInternal(
				LogCategory::Core,
				ErrorCode::InvalidArgument,
				std::string("Duplicate subsystem name: ") + std::string(name));
		}

		indexByName.emplace(name, static_cast<uint32_t>(nodes.size()));
		nodes.push_back(Node{ subsystem.get(), 0, {} });
	}

	for (uint32_t index = 0; index < nodes.size(); ++index)
	{
		const std::string_view name = nodes[index].subsystem->GetName();
		for (const char* const dependencyName : nodes[index].subsystem->GetDependencies())
		{
			if (dependencyName == nullptr || dependencyName[0] == '\0')
			{
				continue;
			}

			const std::string_view dependencyView = dependencyName;
			const auto dependency = indexByName.find(dependencyView);
			if (dependency == indexByName.end())
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

			++nodes[index].inDegree;
			nodes[dependency->second].dependents.push_back(index);
		}
	}

	// Lower registration index is ready first when several dependencies are already satisfied.
	const auto readyLater = [](uint32_t lhs, uint32_t rhs) { return lhs > rhs; };
	std::priority_queue<uint32_t, std::vector<uint32_t>, decltype(readyLater)> ready{ readyLater };
	for (uint32_t index = 0; index < nodes.size(); ++index)
	{
		if (nodes[index].inDegree == 0)
		{
			ready.push(index);
		}
	}

	m_initOrder.reserve(nodes.size());
	while (!ready.empty())
	{
		const uint32_t index = ready.top();
		ready.pop();
		m_initOrder.push_back(nodes[index].subsystem);

		for (const uint32_t dependentIndex : nodes[index].dependents)
		{
			uint32_t& degree = nodes[dependentIndex].inDegree;
			--degree;
			if (degree == 0)
			{
				ready.push(dependentIndex);
			}
		}
	}

	if (m_initOrder.size() != nodes.size())
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
