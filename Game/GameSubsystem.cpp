#include "Game/GameSubsystem.h"

#include "Game/GameHost.h"
#include "Game/IGameModule.h"

#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/Renderer/Scene/RenderSceneTypes.h"

namespace
{
constexpr const char* kDependencies[] = { "Input" };
} // namespace

GameSubsystem::GameSubsystem() = default;

GameSubsystem::~GameSubsystem() = default;

std::unique_ptr<ISubsystem> CreateGameSubsystem()
{
	return std::make_unique<GameSubsystem>();
}

std::span<const char* const> GameSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> GameSubsystem::Initialize(SubsystemContext& ctx)
{
	m_host = std::make_unique<GameHost>(ctx);
	m_module.reset(CreateGameModule());

	if (auto initResult = m_module->OnInit(*m_host); !initResult)
	{
		LogResult(initResult, LogCategory::Core);
		return initResult;
	}

	return MakeOk();
}

Result<void> GameSubsystem::PostInitialize(SubsystemContext& ctx)
{
	if (auto* registry = ctx.GetService<SubsystemRegistry>())
	{
		registry->SetSceneExtractor(this);
	}

	return MakeOk();
}

void GameSubsystem::Tick(SubsystemContext& ctx, float deltaSeconds)
{
	if (m_module != nullptr && m_host != nullptr)
	{
		m_module->OnTick(*m_host, deltaSeconds);
	}
}

void GameSubsystem::Extract(std::vector<ExtractedObject>& outObjects)
{
	if (m_module != nullptr && m_host != nullptr)
	{
		m_module->OnExtract(*m_host, outObjects);
	}
}

void GameSubsystem::Shutdown(SubsystemContext& ctx)
{
	if (m_module != nullptr && m_host != nullptr)
	{
		m_module->OnShutdown(*m_host);
	}

	if (auto* registry = ctx.GetService<SubsystemRegistry>())
	{
		if (registry->GetSceneExtractor() == this)
		{
			registry->SetSceneExtractor(nullptr);
		}
	}

	m_module.reset();
	m_host.reset();
}
