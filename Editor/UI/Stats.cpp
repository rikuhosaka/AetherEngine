#include "Editor/UI/Stats.h"

#include "Runtime/Engine/Application/Subsystem/SubsystemContext.h"
#include "Runtime/Engine/Application/Subsystem/SubsystemRegistry.h"
#include "Runtime/Engine/Engine/Subsystem/Engine.h"
#include "Runtime/Engine/Engine/Subsystem/WindowSubsystem.h"
#include "Runtime/Engine/World/Transform.h"
#include "Runtime/Engine/World/World.h"

#include "imgui.h"

void DrawStats(SubsystemContext& ctx)
{
	const float deltaSeconds = ctx.GetDeltaSeconds();
	const float framesPerSecond = deltaSeconds > 0.0f ? 1.0f / deltaSeconds : 0.0f;
	WindowSubsystem* window = SubsystemRegistry::FindCurrent<WindowSubsystem>();
	const WindowServices* windowServices = window != nullptr ? &window->GetServices() : nullptr;
	const uint32_t clientWidth = windowServices != nullptr ? windowServices->clientWidth : 0;
	const uint32_t clientHeight = windowServices != nullptr ? windowServices->clientHeight : 0;
	Engine* engine = SubsystemRegistry::FindCurrent<Engine>();
	const DebugStatsServices* debugStats = engine != nullptr ? &engine->GetDebugStats() : nullptr;
	const uint32_t extractedObjectCount = debugStats != nullptr ? debugStats->extractedObjectCount : 0;
	WorldServices* worldServices = engine != nullptr ? &engine->GetWorldServices() : nullptr;
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
	if (engine != nullptr)
	{
		SessionServices& session = engine->GetSession();
		const char* modeName = "Editor";
		if (session.mode == SessionMode::Playing)
		{
			modeName = "Playing";
		}
		else if (session.mode == SessionMode::Paused)
		{
			modeName = "Paused";
		}
		ImGui::Text("Mode %s", modeName);
		// 依頼だけを書く。BeginPlay / EndPlay は後続の Engine::Tick が行い、結果は mode に入る。
		// Pause は結果が再生中のときだけ依頼する。停止中の一時停止はセッションを開かずに捨てられる。
		if (ImGui::Button("Play"))
		{
			session.requestMode = SessionMode::Playing;
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause") && session.mode == SessionMode::Playing)
		{
			session.requestMode = SessionMode::Paused;
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
		{
			session.requestMode = SessionMode::Editor;
		}
	}
	ImGui::End();
}
