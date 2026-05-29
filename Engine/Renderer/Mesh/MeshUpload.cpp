#include "Engine/Renderer/Mesh/MeshUpload.h"

#include "Engine/Core/Log/Result.h"
#include "Engine/RHI/Common/RHIResource.h"
#include "Engine/RHI/Interface/RHIBuffer.h"
#include "Engine/RHI/Interface/RHICommandList.h"
#include "Engine/RHI/Interface/RHIDevice.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

namespace
{
size_t GetIndexStride(IndexFormat format)
{
	return format == IndexFormat::R16_UINT ? sizeof(uint16_t) : sizeof(uint32_t);
}
} // namespace

MeshUpload::MeshUpload(RHIDevice* device)
	: m_device(device)
{
}

Result<std::unique_ptr<Mesh>> MeshUpload::CreateMesh(
	const MeshUploadDesc& desc,
	FrameContext& frameContext,
	RHICommandList* commandList) const
{
	if (m_device == nullptr || frameContext.uploadBuffer == nullptr || commandList == nullptr)
	{
		return MakeFail<std::unique_ptr<Mesh>>(
			ErrorCode::InvalidArgument,
			"Mesh upload requires a valid device, upload buffer, and command list");
	}

	const size_t vertexBytes = desc.vertices.size();
	const size_t indexBytes = desc.indices.size();
	if (vertexBytes == 0 || indexBytes == 0 || desc.vertexStride == 0)
	{
		return MakeFail<std::unique_ptr<Mesh>>(
			ErrorCode::InvalidArgument,
			"Mesh upload requires non-empty vertex and index data");
	}

	RHIUploadAllocation vertexAllocation = frameContext.uploadBuffer->Allocate(vertexBytes, 16);
	RHIUploadAllocation indexAllocation = frameContext.uploadBuffer->Allocate(indexBytes, 16);
	if (vertexAllocation.cpuAddress == nullptr || indexAllocation.cpuAddress == nullptr)
	{
		return MakeFail<std::unique_ptr<Mesh>>(
			ErrorCode::OutOfMemory,
			"Upload buffer allocation failed for mesh data");
	}

	std::memcpy(vertexAllocation.cpuAddress, desc.vertices.data(), vertexBytes);
	std::memcpy(indexAllocation.cpuAddress, desc.indices.data(), indexBytes);

	RHIBufferDesc vertexDesc{};
	vertexDesc.Size = vertexBytes;
	vertexDesc.MemoryType = ERHIMemoryType::Default;

	RHIBufferDesc indexDesc{};
	indexDesc.Size = indexBytes;
	indexDesc.MemoryType = ERHIMemoryType::Default;

	auto mesh = std::make_unique<Mesh>();
	auto vertexResult = m_device->CreateVertexBuffer(vertexDesc, desc.vertexStride);
	if (!vertexResult)
	{
		return MakeFail<std::unique_ptr<Mesh>>(
			vertexResult.error.code,
			vertexResult.error.message);
	}

	auto indexResult = m_device->CreateIndexBuffer(indexDesc, desc.indexFormat);
	if (!indexResult)
	{
		return MakeFail<std::unique_ptr<Mesh>>(
			indexResult.error.code,
			indexResult.error.message);
	}

	mesh->vertexBuffer = std::move(vertexResult.value);
	mesh->indexBuffer = std::move(indexResult.value);
	mesh->layoutId = desc.layoutId;
	mesh->submeshes = desc.submeshes;
	mesh->bounds = desc.bounds;

	commandList->CopyBufferRegion(
		mesh->vertexBuffer.get(),
		0,
		frameContext.uploadBuffer,
		vertexAllocation.offset,
		vertexBytes);
	commandList->CopyBufferRegion(
		mesh->indexBuffer.get(),
		0,
		frameContext.uploadBuffer,
		indexAllocation.offset,
		indexBytes);

	commandList->ResourceBarrier(mesh->vertexBuffer.get(), ERHIResourceState::VertexAndConstantBuffer);
	commandList->ResourceBarrier(mesh->indexBuffer.get(), ERHIResourceState::IndexBuffer);

	if (mesh->submeshes.empty() && desc.indexCount > 0)
	{
		SubmeshRange range{};
		range.indexStart = 0;
		range.indexCount = desc.indexCount;
		mesh->submeshes.push_back(range);
	}

	return MakeOk(std::move(mesh));
}
