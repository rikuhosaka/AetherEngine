#include "Engine/Renderer/Scene/RenderConstantsLayout.h"

#include "Engine/Renderer/Material/MaterialTypes.h"

bool MaterialUsesPassConstantBuffers(const Material& material)
{
	for (const ShaderConstantBuffer& layout : material.constantLayout)
	{
		if (layout.Register >= RenderRegisters::MaterialConstants)
		{
			return true;
		}
	}

	return false;
}

bool IsPassBoundConstantRegister(const Material& material, uint32_t registerIndex, uint32_t space)
{
	if (space != 0 || !MaterialUsesPassConstantBuffers(material))
	{
		return false;
	}

	return registerIndex <= RenderRegisters::ObjectConstants;
}
