#include "Editor/UI/Stats.h"

#include "Engine/Application/Services/DebugStatsServices.h"
#include "Engine/Application/Services/EditorSaveServices.h"
#include "Engine/Application/Services/SessionServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Services/WorldServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/World/Transform.h"
#include "Engine/World/World.h"

#include "imgui.h"

void DrawStats(SubsystemContext& ctx)
{
	const float deltaSeconds = ctx.GetDeltaSeconds();
	const float framesPerSecond = deltaSeconds > 0.0f ? 1.0f / deltaSeconds : 0.0f;
	const auto* windowServices = ctx.GetService<WindowServices>();
	const uint32_t clientWidth = windowServices != nullptr ? windowServices->clientWidth : 0;
	const uint32_t clientHeight = windowServices != nullptr ? windowServices->clientHeight : 0;
	const auto* debugStats = ctx.GetService<DebugStatsServices>();
	const uint32_t extractedObjectCount = debugStats != nullptr ? debugStats->extractedObjectCount : 0;
	const auto* worldServices = ctx.GetService<WorldServices>();
	const World* world = worldServices != nullptr ? worldServices->world : nullptr;
	uint32_t entityCount = 0;
	if (world != nullptr)
	{
		world->ForEachAlive([&entityCount](EntityId, const Transform&) {
			++entityCount;
		});
	}

	ImGui::Begin("Stats");
	ImGui::Text("FPS %.1f   %.2f ms", framesPerSecond, deltaSeconds * 1000.0f);
	ImGui::Text("Client %u x %u", clientWidth, clientHeight);
	ImGui::Text("Alpha %.3f", ctx.GetInterpolationAlpha());
	ImGui::Text("Frame slot %u", ctx.GetFrameSlot());
	ImGui::Text("Objects %u", extractedObjectCount);
	ImGui::Text("Entities %u", entityCount);
	if (auto* session = ctx.GetService<SessionServices>())
	{
		const char* modeName = "Editor";
		if (session->mode == SessionMode::Playing)
		{
			modeName = "Playing";
		}
		else if (session->mode == SessionMode::Paused)
		{
			modeName = "Paused";
		}
		ImGui::Text("Mode %s", modeName);
		if (ImGui::Button("Play"))
		{
			session->mode = SessionMode::Playing;
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause") && session->mode == SessionMode::Playing)
		{
			session->mode = SessionMode::Paused;
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
		{
			session->mode = SessionMode::Editor;
		}
		if (auto* editorSave = ctx.GetService<EditorSaveServices>())
		{
			ImGui::SameLine();
			ImGui::BeginDisabled(session->mode != SessionMode::Editor);
			if (ImGui::Button("Save"))
			{
				editorSave->requested = true;
			}
			ImGui::EndDisabled();
		}
	}
	ImGui::End();
}
