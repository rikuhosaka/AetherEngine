#include "Editor/UI/Details.h"

#include "Engine/Application/Services/WorldSelectionServices.h"
#include "Engine/World/Transform.h"
#include "Engine/World/World.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace
{
struct EulerDisplay
{
	EntityId entity{};
	float degrees[3]{};
	bool valid = false;
	bool wasEditable = false;
};

EulerDisplay& EulerDisplayState()
{
	static EulerDisplay display{};
	return display;
}

void EulerDegreesFromQuaternion(const DirectX::XMFLOAT4& rotation, float outDegrees[3])
{
	const DirectX::XMMATRIX matrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
	DirectX::XMFLOAT4X4 stored{};
	DirectX::XMStoreFloat4x4(&stored, matrix);

	const float pitch = std::asinf(std::clamp(-stored._32, -1.0f, 1.0f));
	float yaw = 0.0f;
	float roll = 0.0f;
	if (std::fabs(stored._32) > 0.9999f)
	{
		yaw = std::atan2f(-stored._13, stored._11);
	}
	else
	{
		yaw = std::atan2f(stored._31, stored._33);
		roll = std::atan2f(stored._12, stored._22);
	}

	constexpr float kRadiansToDegrees = 180.0f / DirectX::XM_PI;
	outDegrees[0] = pitch * kRadiansToDegrees;
	outDegrees[1] = yaw * kRadiansToDegrees;
	outDegrees[2] = roll * kRadiansToDegrees;
}

void QuaternionFromEulerDegrees(const float degrees[3], DirectX::XMFLOAT4& rotation)
{
	constexpr float kDegreesToRadians = DirectX::XM_PI / 180.0f;
	const DirectX::XMVECTOR quaternion = DirectX::XMQuaternionRotationRollPitchYaw(
		degrees[0] * kDegreesToRadians,
		degrees[1] * kDegreesToRadians,
		degrees[2] * kDegreesToRadians);
	DirectX::XMStoreFloat4(&rotation, quaternion);
}
} // namespace

void DrawDetails(World* world, WorldSelectionServices* selection, bool editable)
{
	ImGui::Begin("Details");

	EulerDisplay& euler = EulerDisplayState();
	const bool hasSelection = world != nullptr && selection != nullptr && world->IsAlive(selection->selected);
	if (!hasSelection)
	{
		euler = {};
		ImGui::TextUnformatted("Nothing selected");
		ImGui::End();
		return;
	}

	const EntityId id = selection->selected;
	Transform* transform = world->GetTransform(id);
	if (transform == nullptr)
	{
		euler = {};
		ImGui::TextUnformatted("Nothing selected");
		ImGui::End();
		return;
	}

	const std::string_view name = world->GetName(id);
	if (name.empty())
	{
		ImGui::Text("Entity %u:%u", id.Index, id.Generation);
	}
	else
	{
		ImGui::Text("%.*s", static_cast<int>(name.size()), name.data());
	}

	const bool refreshEuler = !editable || !euler.wasEditable || !euler.valid || euler.entity != id;
	if (refreshEuler)
	{
		EulerDegreesFromQuaternion(transform->rotation, euler.degrees);
		euler.entity = id;
		euler.valid = true;
	}

	float position[3] = { transform->position.x, transform->position.y, transform->position.z };
	float scale[3] = { transform->scale.x, transform->scale.y, transform->scale.z };

	ImGui::BeginDisabled(!editable);
	const bool positionChanged = ImGui::DragFloat3("Position", position, 0.01f);
	ImGui::TextUnformatted("Pitch, yaw, roll (degrees)");
	const bool rotationChanged = ImGui::DragFloat3("Rotation", euler.degrees, 1.0f);
	const bool scaleChanged = ImGui::DragFloat3("Scale", scale, 0.01f);
	ImGui::EndDisabled();

	if (positionChanged)
	{
		transform->position = { position[0], position[1], position[2] };
	}
	if (rotationChanged)
	{
		QuaternionFromEulerDegrees(euler.degrees, transform->rotation);
	}
	if (scaleChanged)
	{
		transform->scale = { scale[0], scale[1], scale[2] };
	}

	euler.wasEditable = editable;
	ImGui::End();
}
