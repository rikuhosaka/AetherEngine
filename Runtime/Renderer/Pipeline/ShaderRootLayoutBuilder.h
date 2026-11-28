#pragma once

#include "Runtime/Core/Log/Result.h"
#include "Runtime/Renderer/Pipeline/ShaderRootLayoutTypes.h"
#include "Runtime/Renderer/ShaderSystem/Reflection/ShaderReflectionData.h"

// Maps reflected shader resources onto an existing root signature layout.
// The layout is not modified. Slots are created only for resources the shaders reference.
[[nodiscard]] Result<std::vector<ShaderRootBindingSlot>> MapRootSignatureBindings(
	const RHIRootSignatureLayout& layout,
	std::span<const ShaderReflectionData> stages);
