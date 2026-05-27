#include "Engine/Renderer/Scene/RenderScene.h"

#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionTypes.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
bool IsMeshReady(const Mesh* mesh)
{
	return mesh != nullptr && mesh->vertexBuffer != nullptr && mesh->indexBuffer != nullptr;
}

bool IsTextureReady(const Texture* texture)
{
	return texture != nullptr && texture->resource != nullptr;
}

bool IsMaterialReady(const Material* material)
{
	return material != nullptr && material->vertexShader != nullptr && material->pipelineState.IsValid();
}

uint32_t MakeSortKey(MaterialHandle material, MeshHandle mesh)
{
	return (material.Index << 16) | (mesh.Index & 0xFFFFu);
}

void CopyMatrix(float* destination, const float* source)
{
	std::memcpy(destination, source, sizeof(float) * 16);
}

bool StringEqualsIgnoreCase(std::string_view left, std::string_view right)
{
	if (left.size() != right.size())
	{
		return false;
	}

	for (size_t index = 0; index < left.size(); ++index)
	{
		const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(left[index])));
		const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(right[index])));
		if (a != b)
		{
			return false;
		}
	}

	return true;
}

bool SlotMatchesBaseColor(std::string_view slotName)
{
	return StringEqualsIgnoreCase(slotName, "basecolor")
		|| StringEqualsIgnoreCase(slotName, "albedo")
		|| StringEqualsIgnoreCase(slotName, "g_texture")
		|| StringEqualsIgnoreCase(slotName, "diffuse");
}

bool SlotMatchesNormal(std::string_view slotName)
{
	return StringEqualsIgnoreCase(slotName, "normal")
		|| StringEqualsIgnoreCase(slotName, "normalmap")
		|| StringEqualsIgnoreCase(slotName, "bump");
}

TextureHandle ResolveTextureHandle(
	TextureHandle handle,
	TextureSystemServices& textureServices,
	TextureHandle placeholder)
{
	if (!handle.IsValid())
	{
		return placeholder;
	}

	Texture* texture = textureServices.GetTexture(handle);
	if (!IsTextureReady(texture))
	{
		return placeholder;
	}

	return handle;
}

TextureHandle PickOverrideForSlot(
	std::string_view slotName,
	const StoredMaterialParameterOverrides& overrides,
	TextureSystemServices& textureServices,
	TextureHandle placeholder)
{
	if (SlotMatchesBaseColor(slotName))
	{
		return ResolveTextureHandle(overrides.baseColor, textureServices, placeholder);
	}

	if (SlotMatchesNormal(slotName))
	{
		return ResolveTextureHandle(overrides.normal, textureServices, placeholder);
	}

	return placeholder;
}

std::vector<TextureHandle> BuildBoundTextures(
	const Material& material,
	const StoredMaterialParameterOverrides& overrides,
	TextureSystemServices& textureServices,
	TextureHandle placeholder)
{
	std::vector<TextureHandle> boundTextures;
	for (const ShaderRootBindingSlot& slot : material.bindingSlots)
	{
		if (slot.RootType != RHIRootParamType::SRV)
		{
			continue;
		}

		TextureHandle textureHandle = placeholder;
		for (const MaterialTextureSlot& textureSlot : material.textureSlots)
		{
			if (textureSlot.registerIndex != slot.Register || textureSlot.space != slot.Space)
			{
				continue;
			}

			textureHandle = PickOverrideForSlot(textureSlot.name, overrides, textureServices, placeholder);
			break;
		}

		if (textureHandle == placeholder && !material.textureSlots.empty())
		{
			const size_t srvIndex = boundTextures.size();
			if (srvIndex == 0)
			{
				textureHandle = ResolveTextureHandle(overrides.baseColor, textureServices, placeholder);
			}
			else if (srvIndex == 1)
			{
				textureHandle = ResolveTextureHandle(overrides.normal, textureServices, placeholder);
			}
		}

		boundTextures.push_back(textureHandle);
	}

	if (boundTextures.empty())
	{
		boundTextures.push_back(
			ResolveTextureHandle(overrides.baseColor, textureServices, placeholder));
	}

	return boundTextures;
}

std::vector<std::vector<std::byte>> BuildConstantBuffers(
	const Material& material,
	const StoredMaterialParameterOverrides& overrides)
{
	std::vector<std::vector<std::byte>> constantBuffers;
	constantBuffers.reserve(material.constantLayout.size());
	for (const ShaderConstantBuffer& layout : material.constantLayout)
	{
		constantBuffers.emplace_back(layout.Size, std::byte{ 0 });
	}

	for (const StoredMaterialParameterBlock& block : overrides.parameters)
	{
		if (block.bindingSlot >= constantBuffers.size() || block.data.empty())
		{
			continue;
		}

		std::vector<std::byte>& destination = constantBuffers[block.bindingSlot];
		const size_t copySize = (std::min)(block.data.size(), destination.size());
		std::memcpy(destination.data(), block.data.data(), copySize);
	}

	return constantBuffers;
}

bool ParameterBlocksEqual(
	std::span<const StoredMaterialParameterBlock> left,
	std::span<const StoredMaterialParameterBlock> right)
{
	if (left.size() != right.size())
	{
		return false;
	}

	for (size_t index = 0; index < left.size(); ++index)
	{
		if (left[index].bindingSlot != right[index].bindingSlot || left[index].data != right[index].data)
		{
			return false;
		}
	}

	return true;
}

bool OverridesEqual(
	const StoredMaterialParameterOverrides& left,
	const StoredMaterialParameterOverrides& right)
{
	if (left.baseColor != right.baseColor || left.normal != right.normal)
	{
		return false;
	}

	return ParameterBlocksEqual(left.parameters, right.parameters);
}

bool VectorsEqual(std::span<const TextureHandle> left, std::span<const TextureHandle> right)
{
	if (left.size() != right.size())
	{
		return false;
	}

	for (size_t index = 0; index < left.size(); ++index)
	{
		if (left[index] != right[index])
		{
			return false;
		}
	}

	return true;
}

bool ConstantBuffersEqual(
	std::span<const std::vector<std::byte>> left,
	std::span<const std::vector<std::byte>> right)
{
	if (left.size() != right.size())
	{
		return false;
	}

	for (size_t index = 0; index < left.size(); ++index)
	{
		if (left[index] != right[index])
		{
			return false;
		}
	}

	return true;
}

StoredMaterialParameterOverrides CopyOverrides(const MaterialParameterOverrides& overrides)
{
	StoredMaterialParameterOverrides stored{};
	stored.baseColor = overrides.baseColor;
	stored.normal = overrides.normal;
	stored.parameters.reserve(overrides.parameters.size());
	for (const MaterialParameterBlock& block : overrides.parameters)
	{
		StoredMaterialParameterBlock storedBlock{};
		storedBlock.bindingSlot = block.bindingSlot;
		storedBlock.data.assign(block.data.begin(), block.data.end());
		stored.parameters.push_back(std::move(storedBlock));
	}
	return stored;
}

ExtractedObjectSnapshot CopyExtractedObject(const ExtractedObject& object)
{
	ExtractedObjectSnapshot snapshot{};
	snapshot.objectId = object.objectId;
	snapshot.mesh = object.mesh;
	snapshot.material = object.material;
	snapshot.overrides = CopyOverrides(object.overrides);
	snapshot.submeshIndex = object.submeshIndex;
	CopyMatrix(snapshot.worldMatrix, object.worldMatrix);
	snapshot.layerMask = object.layerMask;
	snapshot.visible = object.visible;
	return snapshot;
}
} // namespace

void RenderScene::BeginFrame()
{
	m_snapshot.opaqueItems.clear();
	m_snapshot.shadowItems.clear();
	m_snapshot.transparentItems.clear();
	m_snapshot.objectConstants.clear();
}

void RenderScene::Extract(std::span<const ExtractedObject> objects, uint32_t frameIndex)
{
	m_extractedFrame.frameIndex = frameIndex;
	m_extractedFrame.objects.clear();
	m_extractedFrame.objects.reserve(objects.size());
	for (const ExtractedObject& object : objects)
	{
		m_extractedFrame.objects.push_back(CopyExtractedObject(object));
	}
}

void RenderScene::Build(
	FrameContext& frameContext,
	RHICommandList* commandList,
	RenderResourceServices& resources)
{
	m_snapshot.frameIndex = m_extractedFrame.frameIndex;
	m_snapshot.opaqueItems.clear();
	m_snapshot.shadowItems.clear();
	m_snapshot.transparentItems.clear();
	m_snapshot.objectConstants.clear();

	std::string placeholderError;
	if (!m_placeholders.EnsureInitialized(
			resources,
			frameContext,
			commandList,
			m_shaderRoot,
			&placeholderError))
	{
		return;
	}

	const RenderPlaceholderResources& placeholders = m_placeholders.Get();
	MeshSystemServices& meshServices = resources.GetMeshServices();
	TextureSystemServices& textureServices = resources.GetTextureServices();
	MaterialSystemServices& materialServices = resources.GetMaterialServices();

	for (const ExtractedObjectSnapshot& object : m_extractedFrame.objects)
	{
		if (!object.visible || !object.objectId.IsValid())
		{
			continue;
		}

		MeshHandle meshHandle = object.mesh;
		Mesh* mesh = meshServices.GetMesh(meshHandle);
		if (!IsMeshReady(mesh))
		{
			meshHandle = placeholders.mesh;
			mesh = meshServices.GetMesh(meshHandle);
		}
		if (!IsMeshReady(mesh))
		{
			continue;
		}

		MaterialHandle materialHandle = object.material;
		Material* material = materialServices.GetMaterial(materialHandle);
		if (!IsMaterialReady(material))
		{
			materialHandle = placeholders.material;
			material = materialServices.GetMaterial(materialHandle);
		}
		if (!IsMaterialReady(material))
		{
			continue;
		}

		const std::vector<TextureHandle> resolvedTextures = BuildBoundTextures(
			*material,
			object.overrides,
			textureServices,
			placeholders.texture);
		const std::vector<std::vector<std::byte>> resolvedConstants =
			BuildConstantBuffers(*material, object.overrides);

		const MaterialInstanceHandle instanceHandle = ResolveMaterialInstance(
			object.objectId,
			materialHandle,
			object.overrides,
			resolvedTextures,
			resolvedConstants,
			resources,
			placeholders);
		if (!instanceHandle.IsValid())
		{
			continue;
		}

		RenderItem item{};
		item.objectId = object.objectId;
		item.mesh = meshHandle;
		item.materialInstance = instanceHandle;
		item.submeshIndex = object.submeshIndex;
		item.sortKey = MakeSortKey(materialHandle, meshHandle);
		item.objectConstantsIndex = static_cast<uint32_t>(m_snapshot.objectConstants.size());

		ObjectConstants constants{};
		CopyMatrix(constants.worldMatrix, object.worldMatrix);
		m_snapshot.objectConstants.push_back(constants);

		if ((object.layerMask & RenderLayer::Opaque) != 0)
		{
			m_snapshot.opaqueItems.push_back(item);
		}
		if ((object.layerMask & RenderLayer::Shadow) != 0)
		{
			m_snapshot.shadowItems.push_back(item);
		}
		if ((object.layerMask & RenderLayer::Transparent) != 0)
		{
			m_snapshot.transparentItems.push_back(item);
		}
	}

	std::ranges::sort(m_snapshot.opaqueItems, {}, &RenderItem::sortKey);
	std::ranges::sort(m_snapshot.shadowItems, {}, &RenderItem::sortKey);
	std::ranges::sort(m_snapshot.transparentItems, {}, &RenderItem::sortKey);
}

MaterialInstanceHandle RenderScene::ResolveMaterialInstance(
	RenderObjectId objectId,
	MaterialHandle material,
	const StoredMaterialParameterOverrides& overrides,
	std::span<const TextureHandle> resolvedTextures,
	std::span<const std::vector<std::byte>> constantBuffers,
	RenderResourceServices& resources,
	const RenderPlaceholderResources& placeholders)
{
	if (!material.IsValid())
	{
		return placeholders.materialInstance;
	}

	MaterialInstanceCacheEntry& cacheEntry = m_instanceCache[objectId];
	const bool cacheHit = cacheEntry.instance.IsValid()
		&& cacheEntry.material == material
		&& OverridesEqual(cacheEntry.overrides, overrides)
		&& VectorsEqual(cacheEntry.resolvedTextures, resolvedTextures)
		&& ConstantBuffersEqual(cacheEntry.constantBuffers, constantBuffers);
	if (cacheHit)
	{
		return cacheEntry.instance;
	}

	std::vector<std::span<const std::byte>> constantSpans;
	constantSpans.reserve(constantBuffers.size());
	for (const std::vector<std::byte>& constantBuffer : constantBuffers)
	{
		constantSpans.emplace_back(constantBuffer);
	}

	const MaterialInstanceHandle instanceHandle = resources.GetMaterialServices().CreateInstance(
		material,
		resolvedTextures,
		constantSpans);
	if (!instanceHandle.IsValid())
	{
		return placeholders.materialInstance;
	}

	cacheEntry.material = material;
	cacheEntry.overrides = overrides;
	cacheEntry.resolvedTextures.assign(resolvedTextures.begin(), resolvedTextures.end());
	cacheEntry.constantBuffers.assign(constantBuffers.begin(), constantBuffers.end());
	cacheEntry.instance = instanceHandle;
	return instanceHandle;
}
