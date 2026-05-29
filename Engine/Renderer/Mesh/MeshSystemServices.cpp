#include "Engine/Renderer/Mesh/MeshSystemServices.h"

Result<std::unique_ptr<MeshSystemServices>> MeshSystemServices::Create(RHIDevice* device)
{
	if (device == nullptr)
	{
		return MakeFail<std::unique_ptr<MeshSystemServices>>(
			ErrorCode::InvalidArgument,
			"MeshSystemServices requires a valid RHIDevice");
	}
	return MakeOk(std::unique_ptr<MeshSystemServices>(new MeshSystemServices(device)));
}

MeshSystemServices::MeshSystemServices(RHIDevice* device)
	: m_upload(device)
{
}

Result<MeshHandle> MeshSystemServices::UploadMesh(
	const MeshUploadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList)
{
	auto meshResult = m_upload.CreateMesh(desc, frameContext, commandList);
	if (!meshResult)
	{
		return MakeFail<MeshHandle>(meshResult.error.code, meshResult.error.message);
	}
	return MakeOk(m_pool.Add(std::move(meshResult.value)));
}
