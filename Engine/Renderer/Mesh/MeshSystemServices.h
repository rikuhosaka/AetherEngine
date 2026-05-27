#pragma once

#include "Engine/Renderer/Mesh/MeshPool.h"
#include "Engine/Renderer/Mesh/MeshUpload.h"

#include <memory>

class RHIDevice;

class MeshSystemServices
{
public:
	static std::unique_ptr<MeshSystemServices> Create(RHIDevice* device);

	[[nodiscard]] MeshPool& GetPool() noexcept { return m_pool; }
	[[nodiscard]] const MeshPool& GetPool() const noexcept { return m_pool; }

	[[nodiscard]] MeshUpload& GetUpload() noexcept { return m_upload; }
	[[nodiscard]] const MeshUpload& GetUpload() const noexcept { return m_upload; }

	[[nodiscard]] MeshHandle UploadMesh(
		const MeshUploadDesc& desc,
		FrameContext& frameContext,
		RHICommandList* commandList);

	[[nodiscard]] Mesh* GetMesh(MeshHandle handle) { return m_pool.Get(handle); }

private:
	explicit MeshSystemServices(RHIDevice* device);

	MeshPool m_pool{};
	MeshUpload m_upload;
};
