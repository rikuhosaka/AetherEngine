#include "Runtime/Engine/EngineInstance/Subsystem/EditorUiSubsystem.h"

#include "Editor/UI/Details.h"
#include "Editor/UI/Outliner.h"
#include "Editor/UI/Stats.h"

#include "Runtime/Engine/Application/Subsystem/SubsystemContext.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Core/Log/Result.h"
#include "Runtime/Engine/EngineInstance/Subsystem/Engine.h"
#include "Runtime/Engine/EngineInstance/Subsystem/UiSubsystem.h"
#include "Runtime/Engine/World/World.h"

namespace
{
constexpr const char* kDependencies[] = { "UI" };
} // namespace

std::unique_ptr<ISubsystem> CreateEditorUiSubsystem()
{
	return std::make_unique<EditorUiSubsystem>();
}

std::span<const char* const> EditorUiSubsystem::GetDependencies() const
{
	return kDependencies;
}

Result<void> EditorUiSubsystem::Initialize(SubsystemContext& /*ctx*/)
{
	return MakeOk();
}

void EditorUiSubsystem::Tick(SubsystemContext& ctx, float /*deltaSeconds*/)
{
	UiSubsystem* ui = SubsystemRegistry::FindCurrent<UiSubsystem>();
	const UiServices* uiServices = ui != nullptr ? &ui->GetUi() : nullptr;
	if (uiServices == nullptr || !uiServices->frameStarted)
	{
		return;
	}

	Engine* engine = SubsystemRegistry::FindCurrent<Engine>();
	WorldServices* worldServices = engine != nullptr ? &engine->GetWorldServices() : nullptr;
	const uint32_t worldGeneration = worldServices != nullptr ? worldServices->generation : 0;
	if (m_selection.worldGeneration != worldGeneration)
	{
		m_selection.selected = {};
		m_selection.worldGeneration = worldGeneration;
	}

	DrawStats(ctx);

	const SessionServices* session = engine != nullptr ? &engine->GetSession() : nullptr;
	const bool editable = session != nullptr && session->mode == SessionMode::Editor;
	const World* world = worldServices != nullptr ? worldServices->world : nullptr;
	DrawOutliner(world, &m_selection);
	DrawDetails(worldServices != nullptr ? worldServices->world : nullptr, &m_selection, editable);
}

void EditorUiSubsystem::Shutdown(SubsystemContext& /*ctx*/)
{
	m_selection = {};
}
