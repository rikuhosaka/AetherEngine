#include "Editor/UI/EditorUiSubsystem.h"

#include "Editor/UI/Details.h"
#include "Editor/UI/Outliner.h"
#include "Editor/UI/Stats.h"

#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/UiServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/World/World.h"

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

Result<void> EditorUiSubsystem::Initialize(SubsystemContext& ctx)
{
	ctx.RegisterService(&m_selection);
	return MakeOk();
}

void EditorUiSubsystem::Tick(SubsystemContext& ctx, float /*deltaSeconds*/)
{
	const auto* uiServices = ctx.GetService<UiServices>();
	if (uiServices == nullptr || uiServices->imgui == nullptr)
	{
		return;
	}

	auto* worldServices = ctx.GetService<WorldServices>();
	const uint32_t worldGeneration = worldServices != nullptr ? worldServices->generation : 0;
	if (m_selection.worldGeneration != worldGeneration)
	{
		m_selection.selected = {};
		m_selection.worldGeneration = worldGeneration;
	}

	DrawStats(ctx);

	const auto* session = ctx.GetService<SessionServices>();
	const bool editable = session != nullptr && session->mode == SessionMode::Editor;
	const World* world = worldServices != nullptr ? worldServices->world : nullptr;
	DrawOutliner(world, &m_selection);
	DrawDetails(worldServices != nullptr ? worldServices->world : nullptr, &m_selection, editable);
}

void EditorUiSubsystem::Shutdown(SubsystemContext& ctx)
{
	ctx.UnregisterService(&m_selection);
	m_selection = {};
}
