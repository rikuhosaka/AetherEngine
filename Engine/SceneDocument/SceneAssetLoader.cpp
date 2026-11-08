#include "Engine/SceneDocument/SceneAssetLoader.h"

#include "Engine/AssetSystem/AssetCache.h"
#include "Engine/Core/Log/LogMacros.h"
#include "Engine/SceneDocument/SceneDocument.h"

Result<void> ApplyMeshCollision(SceneDocument& document, const AssetCache& cache)
{
	for (SceneEntity& entity : document.entities)
	{
		if (!entity.body.has_value())
		{
			continue;
		}

		RigidBodyShape& shape = entity.body->shape;
		if (shape.type != PhysicsShapeType::Mesh && shape.type != PhysicsShapeType::ConvexHull)
		{
			continue;
		}

		if (shape.mesh.empty())
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene mesh collision asset is empty");
		}

		const MeshAsset* asset = cache.Find(shape.mesh);
		if (asset == nullptr)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene mesh collision asset was not found: " + shape.mesh);
		}

		const bool triangle = shape.type == PhysicsShapeType::Mesh && asset->kind == MeshAssetKind::TriangleList;
		const bool hull = shape.type == PhysicsShapeType::ConvexHull && asset->kind == MeshAssetKind::ConvexHull;
		if (!triangle && !hull)
		{
			return FailRuntime(
				LogCategory::Asset,
				ErrorCode::InvalidArgument,
				"Scene mesh collision asset kind does not match the shape");
		}

		shape.points = asset->points;
		shape.indices = asset->indices;
	}

	return MakeOk();
}
