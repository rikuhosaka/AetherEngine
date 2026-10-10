#include "Engine/Renderer/Scene/RenderScene.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Math/Matrix.h"
#include "Engine/Renderer/Scene/FrustumCull.h"
#include "Engine/Renderer/Scene/RenderConstantsBuild.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Material/MaterialTypes.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/ShaderSystem/Reflection/ShaderReflectionTypes.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
constexpr uint32_t kMaterialInstanceCacheUnusedFrameLimit = 3;

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
		|| StringEqualsIgnoreCase(slotName, "g_normalmap")
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
	TextureHandle albedoPlaceholder,
	TextureHandle normalPlaceholder)
{
	if (SlotMatchesBaseColor(slotName))
	{
		return ResolveTextureHandle(overrides.baseColor, textureServices, albedoPlaceholder);
	}

	if (SlotMatchesNormal(slotName))
	{
		return ResolveTextureHandle(overrides.normal, textureServices, normalPlaceholder);
	}

	return albedoPlaceholder;
}

std::vector<TextureHandle> BuildBoundTextures(
	const Material& material,
	const StoredMaterialParameterOverrides& overrides,
	TextureSystemServices& textureServices,
	TextureHandle albedoPlaceholder,
	TextureHandle normalPlaceholder)
{
	std::vector<TextureHandle> boundTextures;
	for (const ShaderRootBindingSlot& slot : material.bindingSlots)
	{
		if (slot.RootType != RHIRootParamType::SRV)
		{
			continue;
		}

		const uint32_t descriptorCount = slot.BindCount == 0 ? 1u : slot.BindCount;
		for (uint32_t element = 0; element < descriptorCount; ++element)
		{
			const uint32_t shaderRegister = slot.Register + element;
			if (IsPassBoundShaderResource(shaderRegister, slot.Space))
			{
				continue;
			}

			const MaterialTextureSlot* textureSlot = nullptr;
			for (const MaterialTextureSlot& candidate : material.textureSlots)
			{
				if (candidate.registerIndex == shaderRegister && candidate.space == slot.Space)
				{
					textureSlot = &candidate;
					break;
				}
			}

			TextureHandle textureHandle = albedoPlaceholder;
			if (textureSlot != nullptr)
			{
				textureHandle = PickOverrideForSlot(
					textureSlot->name,
					overrides,
					textureServices,
					albedoPlaceholder,
					normalPlaceholder);
			}
			else if (shaderRegister != 0)
			{
				textureHandle = ResolveTextureHandle(overrides.normal, textureServices, normalPlaceholder);
			}
			else
			{
				textureHandle = ResolveTextureHandle(overrides.baseColor, textureServices, albedoPlaceholder);
			}

			boundTextures.push_back(textureHandle);
		}
	}

	if (boundTextures.empty())
	{
		boundTextures.push_back(
			ResolveTextureHandle(overrides.baseColor, textureServices, albedoPlaceholder));
	}

	return boundTextures;
}

float ObjectDistanceSquared(const ObjectConstants& constants, const ExtractedView& view)
{
	const DirectX::XMMATRIX world = Aether::Math::LoadMatrixFromHlsl(constants.worldMatrix);
	DirectX::XMFLOAT4X4 stored{};
	DirectX::XMStoreFloat4x4(&stored, world);
	const float deltaX = stored._41 - view.cameraPosition[0];
	const float deltaY = stored._42 - view.cameraPosition[1];
	const float deltaZ = stored._43 - view.cameraPosition[2];
	return (deltaX * deltaX) + (deltaY * deltaY) + (deltaZ * deltaZ);
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
	m_snapshot.hasView = false;
	m_snapshot.hasLighting = false;
	m_hasExtractedView = false;
	m_hasExtractedLighting = false;
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

void RenderScene::ExtractView(const ExtractedView& view)
{
	m_extractedView = view;
	m_hasExtractedView = true;
}

void RenderScene::ExtractLighting(const ExtractedLighting& lighting)
{
	m_extractedLighting = lighting;
	m_hasExtractedLighting = true;
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
	m_snapshot.hasView = m_hasExtractedView;
	m_snapshot.hasLighting = m_hasExtractedLighting;

	if (m_hasExtractedView)
	{
		m_snapshot.view = m_extractedView;
	}

	if (m_hasExtractedLighting)
	{
		m_snapshot.lighting = m_extractedLighting;
	}

	if (m_snapshot.hasView && m_snapshot.hasLighting)
	{
		BuildFrameConstants(m_snapshot.view, m_snapshot.lighting, m_snapshot.frameConstants);
	}

	std::string placeholderError;
	if (!m_placeholders.EnsureInitialized(
			resources,
			frameContext,
			commandList,
			m_shaderRoot,
			&placeholderError))
	{
		LOG_ERROR(LogCategory::Renderer,
			placeholderError.empty() ? "Failed to initialize render placeholders" : placeholderError);
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

		// Camera-frustum culling also drops shadow casters that sit outside the view.
		if (m_snapshot.hasView && AreBoundsCullable(mesh->bounds) &&
			!IntersectsViewFrustum(m_snapshot.view.viewProjectionMatrix, object.worldMatrix, mesh->bounds))
		{
			continue;
		}

		const std::vector<TextureHandle> resolvedTextures = BuildBoundTextures(
			*material,
			object.overrides,
			textureServices,
			placeholders.texture,
			placeholders.normalTexture);
		const std::vector<std::vector<std::byte>> resolvedConstants =
			BuildConstantBuffers(*material, object.overrides);

		const MaterialInstanceHandle instanceHandle = ResolveMaterialInstance(
			object.objectId,
			materialHandle,
			object.overrides,
			resolvedTextures,
			resolvedConstants,
			resources,
			placeholders,
			m_extractedFrame.frameIndex);
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
		BuildObjectConstants(object.worldMatrix, constants);
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
	if (m_snapshot.hasView)
	{
		std::ranges::sort(m_snapshot.transparentItems, [&](const RenderItem& left, const RenderItem& right) {
			const float leftDistance = left.objectConstantsIndex < m_snapshot.objectConstants.size()
				? ObjectDistanceSquared(m_snapshot.objectConstants[left.objectConstantsIndex], m_snapshot.view)
				: 0.0f;
			const float rightDistance = right.objectConstantsIndex < m_snapshot.objectConstants.size()
				? ObjectDistanceSquared(m_snapshot.objectConstants[right.objectConstantsIndex], m_snapshot.view)
				: 0.0f;
			return leftDistance > rightDistance;
		});
	}
	else
	{
		std::ranges::sort(m_snapshot.transparentItems, {}, &RenderItem::sortKey);
	}

	ReleaseStaleMaterialInstances(materialServices, m_extractedFrame.frameIndex);
}

MaterialInstanceHandle RenderScene::ResolveMaterialInstance(
	RenderObjectId objectId,
	MaterialHandle material,
	const StoredMaterialParameterOverrides& overrides,
	std::span<const TextureHandle> resolvedTextures,
	std::span<const std::vector<std::byte>> constantBuffers,
	RenderResourceServices& resources,
	const RenderPlaceholderResources& placeholders,
	uint32_t frameIndex)
{
	if (!material.IsValid())
	{
		return placeholders.materialInstance;
	}

	MaterialSystemServices& materialServices = resources.GetMaterialServices();
	const auto cacheIt = m_instanceCache.find(objectId);
	const bool hasCacheEntry = cacheIt != m_instanceCache.end();
	const bool cacheHit = hasCacheEntry
		&& cacheIt->second.instance.IsValid()
		&& cacheIt->second.material == material
		&& OverridesEqual(cacheIt->second.overrides, overrides)
		&& VectorsEqual(cacheIt->second.resolvedTextures, resolvedTextures)
		&& ConstantBuffersEqual(cacheIt->second.constantBuffers, constantBuffers);
	if (cacheHit)
	{
		cacheIt->second.lastUsedFrame = frameIndex;
		return cacheIt->second.instance;
	}

	std::vector<std::span<const std::byte>> constantSpans;
	constantSpans.reserve(constantBuffers.size());
	for (const std::vector<std::byte>& constantBuffer : constantBuffers)
	{
		constantSpans.emplace_back(constantBuffer);
	}

	const MaterialInstanceHandle instanceHandle = materialServices.CreateInstance(
		material,
		resolvedTextures,
		constantSpans);
	if (!instanceHandle.IsValid())
	{
		return placeholders.materialInstance;
	}

	MaterialInstanceCacheEntry& cacheEntry = hasCacheEntry
		? cacheIt->second
		: m_instanceCache[objectId];
	if (cacheEntry.instance.IsValid())
	{
		materialServices.DestroyInstance(cacheEntry.instance);
	}

	cacheEntry.material = material;
	cacheEntry.overrides = overrides;
	cacheEntry.resolvedTextures.assign(resolvedTextures.begin(), resolvedTextures.end());
	cacheEntry.constantBuffers.assign(constantBuffers.begin(), constantBuffers.end());
	cacheEntry.instance = instanceHandle;
	cacheEntry.lastUsedFrame = frameIndex;
	return instanceHandle;
}

void RenderScene::ReleaseStaleMaterialInstances(MaterialSystemServices& materialServices, uint32_t frameIndex)
{
	for (auto cacheIt = m_instanceCache.begin(); cacheIt != m_instanceCache.end();)
	{
		const uint32_t unusedFrames = frameIndex - cacheIt->second.lastUsedFrame;
		if (unusedFrames <= kMaterialInstanceCacheUnusedFrameLimit)
		{
			++cacheIt;
			continue;
		}

		if (cacheIt->second.instance.IsValid())
		{
			materialServices.DestroyInstance(cacheIt->second.instance);
		}
		cacheIt = m_instanceCache.erase(cacheIt);
	}
}
