#include "Engine/Application/Test/SubsystemOrderTest.h"

#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/LogMacros.h"

#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace
{
class OrderProbe final : public ISubsystem
{
public:
	OrderProbe(const char* name, std::span<const char* const> dependencies, std::vector<const char*>* order)
		: m_name(name)
		, m_dependencies(dependencies)
		, m_order(order)
	{
	}

	[[nodiscard]] const char* GetName() const override { return m_name; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override { return m_dependencies; }

	Result<void> Initialize(SubsystemContext&) override
	{
		if (m_order != nullptr)
		{
			m_order->push_back(m_name);
		}

		return MakeOk();
	}

	void Shutdown(SubsystemContext&) override
	{
	}

private:
	const char* m_name = nullptr;
	std::span<const char* const> m_dependencies{};
	std::vector<const char*>* m_order = nullptr;
};

[[nodiscard]] int IndexOf(const std::vector<const char*>& order, const char* name)
{
	for (int index = 0; index < static_cast<int>(order.size()); ++index)
	{
		if (std::strcmp(order[static_cast<size_t>(index)], name) == 0)
		{
			return index;
		}
	}

	return -1;
}
} // namespace

Result<void> RunSubsystemOrderTests()
{
	constexpr const char* kSceneDependencies[] = { "Render", "Input" };
	constexpr const char* kEditorDependencies[] = { "Window", "Scene" };

	std::vector<const char*> order{};
	SubsystemRegistry registry{};
	registry.Register(std::make_unique<OrderProbe>("Editor", kEditorDependencies, &order));
	registry.Register(std::make_unique<OrderProbe>("Scene", kSceneDependencies, &order));
	registry.Register(std::make_unique<OrderProbe>("Input", std::span<const char* const>{}, &order));
	registry.Register(std::make_unique<OrderProbe>("Render", std::span<const char* const>{}, &order));
	registry.Register(std::make_unique<OrderProbe>("Window", std::span<const char* const>{}, &order));

	SubsystemContext context{};
	if (auto initialized = registry.InitializeAll(context); !initialized)
	{
		return MakeFail(initialized.error.code, initialized.error.message);
	}

	const int editor = IndexOf(order, "Editor");
	const int scene = IndexOf(order, "Scene");
	const int input = IndexOf(order, "Input");
	const int render = IndexOf(order, "Render");
	const int window = IndexOf(order, "Window");
	if (editor < 0 || scene < 0 || input < 0 || render < 0 || window < 0
		|| editor < window || editor < scene
		|| scene < render || scene < input
		|| !(input < render && render < window))
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"Subsystem init order should follow dependencies, then registration order");
	}

	constexpr const char* kMissingDependency[] = { "Missing" };
	SubsystemRegistry unknown{};
	unknown.Register(std::make_unique<OrderProbe>("Scene", kMissingDependency, nullptr));
	if (unknown.InitializeAll(context))
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"A dependency on an unknown subsystem should fail");
	}

	constexpr const char* kCycleFromA[] = { "B" };
	constexpr const char* kCycleFromB[] = { "A" };
	SubsystemRegistry cycle{};
	cycle.Register(std::make_unique<OrderProbe>("A", kCycleFromA, nullptr));
	cycle.Register(std::make_unique<OrderProbe>("B", kCycleFromB, nullptr));
	if (cycle.InitializeAll(context))
	{
		return FailRuntime(
			LogCategory::Core,
			ErrorCode::InvalidArgument,
			"A subsystem dependency cycle should fail");
	}

	LOG_INFO(LogCategory::Core, "SubsystemOrderTests passed");
	return MakeOk();
}
