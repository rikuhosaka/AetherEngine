#include "Engine/Renderer/Mesh/MeshSystemServices.h"

std::unique_ptr<MeshSystemServices> MeshSystemServices::Create(RHIDevice* device)
{
	if (device == nullptr)
	{
		return nullptr;
	}
	return std::unique_ptr<MeshSystemServices>(new MeshSystemServices(device));
}

MeshSystemServices::MeshSystemServices(RHIDevice* device)
	: m_upload(device)
{
}

MeshHandle MeshSystemServices::UploadMesh(
	const MeshUploadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	std::unique_ptr<Mesh> mesh = m_upload.CreateMesh(desc, frameContext, commandList);
	if (mesh == nullptr)
	{
		return {};
	}
	return m_pool.Add(std::move(mesh));
}
