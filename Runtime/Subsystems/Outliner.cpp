#include "Runtime/Subsystems/Outliner.h"

#include "Engine/Application/Services/WorldSelectionServices.h"
#include "Engine/World/World.h"

#include "imgui.h"

#include <string_view>
#include <vector>

namespace
{
constexpr int kMaxOutlinerDepth = 64;

void DrawEntity(const World& world, EntityId id, WorldSelectionServices* selection, int depth)
{
	if (depth >= kMaxOutlinerDepth)
	{
		return;
	}

	std::vector<EntityId> children;
	world.ForEachChild(id, [&children](EntityId child) {
		children.push_back(child);
	});

	const bool leaf = children.empty() || depth + 1 >= kMaxOutlinerDepth;
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (leaf)
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (selection != nullptr && selection->selected == id)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	ImGui::PushID(static_cast<int>(id.Index));
	ImGui::PushID(static_cast<int>(id.Generation));

	const std::string_view name = world.GetName(id);
	const bool open = name.empty()
		? ImGui::TreeNodeEx("##entity", flags, "Entity %u:%u", id.Index, id.Generation)
		: ImGui::TreeNodeEx("##entity", flags, "%.*s", static_cast<int>(name.size()), name.data());
	if (selection != nullptr && ImGui::IsItemClicked())
	{
		selection->selected = id;
	}

	if (open && !leaf)
	{
		for (const EntityId child : children)
		{
			DrawEntity(world, child, selection, depth + 1);
		}
		ImGui::TreePop();
	}

	ImGui::PopID();
	ImGui::PopID();
}
} // namespace

void DrawOutliner(const World* world, WorldSelectionServices* selection)
{
	if (selection != nullptr && (world == nullptr || !world->IsAlive(selection->selected)))
	{
		selection->selected = {};
	}

	ImGui::Begin("Outliner");
	if (world != nullptr)
	{
		world->ForEachRoot([&world, selection](EntityId id) {
			DrawEntity(*world, id, selection, 0);
		});
	}
	ImGui::End();
}
