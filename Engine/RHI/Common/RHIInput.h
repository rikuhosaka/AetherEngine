#pragma once

#include <cstddef>
#include <cstdint>

enum class InputLayoutType
{
	Basic,
	PositionTex,
	Skinned,
	// Dear ImGui ImDrawVert: pos, uv, packed RGBA. 20 bytes.
	ImGui,
	None
};

// Matches the default ImDrawVert layout (no IMGUI_OVERRIDE_DRAWVERT_STRUCT_LAYOUT).
// color is little-endian R,G,B,A, which binds as R8G8B8A8_UNORM.
struct ImGuiVertex
{
	float position[2]{};
	float uv[2]{};
	std::uint32_t color = 0;
};

static_assert(sizeof(ImGuiVertex) == 20);
static_assert(offsetof(ImGuiVertex, position) == 0);
static_assert(offsetof(ImGuiVertex, uv) == 8);
static_assert(offsetof(ImGuiVertex, color) == 16);

enum class PrimitiveTopology
{
	TriangleList,
	TriangleStrip,
	LineList,
	PointList
};

enum class IndexFormat
{
	R16_UINT,
	R32_UINT
};