#include "Engine/Scene/SceneAssetLoader.h"

#include "Engine/Renderer/Material/MaterialSystemServices.h"
#include "Engine/Renderer/Mesh/MeshSystemServices.h"
#include "Engine/Renderer/Mesh/PrimitiveMesh.h"
#include "Engine/Renderer/Resource/RenderResourceServices.h"
#include "Engine/Renderer/Scene/RenderConstantsLayout.h"
#include "Engine/Renderer/Texture/TextureSystemServices.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/Scene/WorldRenderBridge.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr uint32_t kSphereStacks = 12;
constexpr uint32_t kSphereSlices = 16;
constexpr uint32_t kCapsuleSlices = 16;
constexpr uint32_t kCapsuleHemisphereStacks = 4;

[[nodiscard]] bool SameFloat3(const DirectX::XMFLOAT3& left, const DirectX::XMFLOAT3& right)
{
	return left.x == right.x && left.y == right.y && left.z == right.z;
}

[[nodiscard]] bool SameShape(const SceneShape& left, const SceneShape& right)
{
	if (left.type != right.type || !SameFloat3(left.halfExtent, right.halfExtent) || left.radius != right.radius
		|| left.halfHeight != right.halfHeight || left.points.size() != right.points.size()
		|| left.indices.size() != right.indices.size())
	{
		return false;
	}

	for (size_t index = 0; index < left.points.size(); ++index)
	{
		if (!SameFloat3(left.points[index], right.points[index]))
		{
			return false;
		}
	}

	for (size_t index = 0; index < left.indices.size(); ++index)
	{
		if (left.indices[index] != right.indices[index])
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool ContainsKey(const std::vector<std::string>& keys, std::string_view key)
{
	for (const std::string& existing : keys)
	{
		if (existing == key)
		{
			return true;
		}
	}

	return false;
}

[[nodiscard]] const SceneShape* ShapeForKey(const SceneDocument& document, std::string_view key, bool& conflict)
{
	const SceneShape* shape = nullptr;
	for (const SceneEntity& entity : document.entities)
	{
		if (entity.mesh != key || !entity.body.has_value())
		{
			continue;
		}

		if (shape == nullptr)
		{
			shape = &entity.body->shape;
			continue;
		}

		if (!SameShape(*shape, entity.body->shape))
		{
			conflict = true;
			return nullptr;
		}
	}

	conflict = false;
	return shape;
}

[[nodiscard]] Result<PrimitiveMeshData> GenerateShapeMesh(const SceneShape& shape)
{
	switch (shape.type)
	{
	case PhysicsShapeType::Box:
		return GenerateCubeMesh({
			shape.halfExtent.x * 2.0f,
			shape.halfExtent.y * 2.0f,
			shape.halfExtent.z * 2.0f,
			false });
	case PhysicsShapeType::Sphere:
		return GenerateSphereMesh({ shape.radius, kSphereStacks, kSphereSlices });
	case PhysicsShapeType::Capsule:
		return GenerateCapsuleMesh({
			shape.radius,
			shape.halfHeight,
			kCapsuleSlices,
			kCapsuleHemisphereStacks });
	case PhysicsShapeType::ConvexHull:
		return GenerateConvexHullMesh({ shape.points });
	case PhysicsShapeType::Mesh:
		return GenerateTriangleMesh({ shape.points, shape.indices });
	default:
		return FailInternal<PrimitiveMeshData>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Scene asset mesh key has an unsupported shape");
	}
}

[[nodiscard]] Result<MeshHandle> UploadPrimitive(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const PrimitiveMeshData& mesh,
	const char* debugName)
{
	return resources.GetMeshServices().UploadMesh(
		MakePrimitiveMeshUploadDesc(mesh, debugName),
		frameContext,
		commandList);
}

[[nodiscard]] Result<TextureHandle> UploadWhiteTexture(
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	const std::array<std::byte, 4> whitePixel = {
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
		std::byte{ 255 },
	};
	TextureUploadDesc textureDesc{};
	textureDesc.width = 1;
	textureDesc.height = 1;
	TextureMipData mip{};
	mip.width = 1;
	mip.height = 1;
	mip.rowPitch = 4;
	mip.pixels = whitePixel;
	textureDesc.mips.push_back(mip);

	return resources.GetTextureServices().UploadTexture(textureDesc, frameContext, commandList);
}
} // namespace

Result<SceneAssets> SceneAssetLoader::Load(
	const SceneDocument& document,
	RenderResourceServices& resources,
	FrameContext& frameContext,
	RHICommandList* commandList,
	const std::filesystem::path& shaderRoot)
{
	if (commandList == nullptr)
	{
		return FailInternal<SceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"SceneAssetLoader requires a valid command list");
	}

	if (shaderRoot.empty())
	{
		return FailInternal<SceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"SceneAssetLoader requires a valid shader root path");
	}

	std::vector<std::string> keys{};
	for (const SceneEntity& entity : document.entities)
	{
		if (entity.mesh.empty())
		{
			return FailInternal<SceneAssets>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh key is empty");
		}

		if (!ContainsKey(keys, entity.mesh))
		{
			keys.push_back(entity.mesh);
		}
	}

	SceneAssets assets{};
	assets.meshes.reserve(keys.size());
	for (const std::string& key : keys)
	{
		bool conflict = false;
		const SceneShape* shape = ShapeForKey(document, key, conflict);
		if (conflict)
		{
			return FailInternal<SceneAssets>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh key has conflicting shapes");
		}

		if (shape == nullptr)
		{
			return FailInternal<SceneAssets>(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene asset mesh key has no shape");
		}

		const Result<PrimitiveMeshData> mesh = GenerateShapeMesh(*shape);
		if (!mesh)
		{
			return MakeFail<SceneAssets>(mesh.error.code, mesh.error.message);
		}

		const Result<MeshHandle> uploaded = UploadPrimitive(
			resources, frameContext, commandList, mesh.value, key.c_str());
		if (!uploaded)
		{
			return MakeFail<SceneAssets>(uploaded.error.code, uploaded.error.message);
		}

		assets.meshes.push_back(SceneMeshRef{ key, ToWorldMeshId(uploaded.value) });
	}

	const Result<TextureHandle> texture = UploadWhiteTexture(resources, frameContext, commandList);
	if (!texture)
	{
		return MakeFail<SceneAssets>(texture.error.code, texture.error.message);
	}

	MaterialCreateDesc materialDesc{};
	materialDesc.vertexShaderPath = shaderRoot / "LitBasicVS.hlsl";
	materialDesc.pixelShaderPath = shaderRoot / "LitBasicPS.hlsl";
	materialDesc.vertexEntryPoint = "LitBasicVS";
	materialDesc.pixelEntryPoint = "LitBasicPS";
	materialDesc.inputLayout = InputLayoutType::Basic;
	materialDesc.requiredLayout = VertexLayoutId::Basic;

	const Result<MaterialHandle> material = resources.GetMaterialServices().CreateMaterial(materialDesc);
	if (!material)
	{
		return MakeFail<SceneAssets>(material.error.code, material.error.message);
	}

	const Material* createdMaterial = resources.GetMaterialServices().GetMaterial(material.value);
	if (createdMaterial == nullptr)
	{
		return FailInternal<SceneAssets>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"SceneAssetLoader failed to resolve created material");
	}

	assets.render.material = ToWorldMaterialId(material.value);
	assets.render.baseColor = ToWorldTextureId(texture.value);
	for (uint32_t layoutIndex = 0; layoutIndex < createdMaterial->constantLayout.size(); ++layoutIndex)
	{
		const ShaderConstantBuffer& layout = createdMaterial->constantLayout[layoutIndex];
		if (layout.Register == RenderRegisters::MaterialConstants && layout.Space == 0)
		{
			assets.render.materialConstantsSlot = layoutIndex;
			break;
		}
	}

	return MakeOk(std::move(assets));
}
