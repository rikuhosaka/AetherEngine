#pragma once

#include "Engine/RHI/Common/RHIInput.h"
#include "Engine/RHI/Interface/RHIBuffer.h"
#include "Engine/RHI/Interface/RHIUploadBuffer.h"

#include <cstddef>
#include <cstdint>
#include <optional>

struct RHIVertexBufferView
{
	uint64_t gpuAddress = 0;
	uint32_t sizeInBytes = 0;
	uint32_t strideInBytes = 0;

	[[nodiscard]] static RHIVertexBufferView FromBuffer(
		const RHIVertexBuffer* buffer,
		uint32_t byteOffset = 0,
		std::optional<uint32_t> sizeInBytes = std::nullopt);

	[[nodiscard]] static RHIVertexBufferView FromUpload(
		const RHIUploadAllocation& allocation,
		uint32_t strideInBytes,
		uint32_t byteOffset = 0,
		std::optional<uint32_t> sizeInBytes = std::nullopt);
};

struct RHIIndexBufferView
{
	uint64_t gpuAddress = 0;
	uint32_t sizeInBytes = 0;
	IndexFormat format = IndexFormat::R16_UINT;

	[[nodiscard]] static RHIIndexBufferView FromBuffer(
		const RHIIndexBuffer* buffer,
		uint32_t byteOffset = 0,
		std::optional<uint32_t> sizeInBytes = std::nullopt);

	[[nodiscard]] static RHIIndexBufferView FromUpload(
		const RHIUploadAllocation& allocation,
		IndexFormat format,
		uint32_t byteOffset = 0,
		std::optional<uint32_t> sizeInBytes = std::nullopt);
};

namespace RHIBufferViewDetail
{
[[nodiscard]] inline uint32_t IndexElementStride(IndexFormat format)
{
	switch (format)
	{
	case IndexFormat::R16_UINT:
		return sizeof(uint16_t);
	case IndexFormat::R32_UINT:
		return sizeof(uint32_t);
	default:
		return 0;
	}
}

[[nodiscard]] inline bool ResolveByteRange(
	uint64_t baseGpuAddress,
	size_t resourceSize,
	uint32_t elementStride,
	uint32_t byteOffset,
	std::optional<uint32_t> sizeInBytes,
	uint64_t& outGpuAddress,
	uint32_t& outSizeInBytes)
{
	if (baseGpuAddress == 0 || elementStride == 0 || resourceSize == 0)
	{
		return false;
	}

	if (static_cast<size_t>(byteOffset) >= resourceSize)
	{
		return false;
	}

	const size_t available = resourceSize - static_cast<size_t>(byteOffset);
	const size_t requested = sizeInBytes.has_value()
		? static_cast<size_t>(*sizeInBytes)
		: available;
	constexpr size_t kMaxUint32 = static_cast<size_t>(UINT32_MAX);
	if (requested == 0
		|| requested > available
		|| requested > kMaxUint32
		|| requested % elementStride != 0)
	{
		return false;
	}

	if (static_cast<uint64_t>(byteOffset) > UINT64_MAX - baseGpuAddress)
	{
		return false;
	}

	outGpuAddress = baseGpuAddress + byteOffset;
	outSizeInBytes = static_cast<uint32_t>(requested);
	return true;
}
} // namespace RHIBufferViewDetail

inline RHIVertexBufferView RHIVertexBufferView::FromBuffer(
	const RHIVertexBuffer* buffer,
	uint32_t byteOffset,
	std::optional<uint32_t> sizeInBytes)
{
	RHIVertexBufferView view{};
	if (buffer == nullptr)
	{
		return view;
	}

	view.strideInBytes = buffer->GetStride();
	if (!RHIBufferViewDetail::ResolveByteRange(
			buffer->GetGPUAddress(),
			buffer->GetSize(),
			view.strideInBytes,
			byteOffset,
			sizeInBytes,
			view.gpuAddress,
			view.sizeInBytes))
	{
		return {};
	}

	return view;
}

inline RHIVertexBufferView RHIVertexBufferView::FromUpload(
	const RHIUploadAllocation& allocation,
	uint32_t strideInBytes,
	uint32_t byteOffset,
	std::optional<uint32_t> sizeInBytes)
{
	RHIVertexBufferView view{};
	view.strideInBytes = strideInBytes;
	if (allocation.cpuAddress == nullptr)
	{
		return {};
	}

	if (!RHIBufferViewDetail::ResolveByteRange(
			allocation.gpuVirtualAddress,
			allocation.size,
			strideInBytes,
			byteOffset,
			sizeInBytes,
			view.gpuAddress,
			view.sizeInBytes))
	{
		return {};
	}

	return view;
}

inline RHIIndexBufferView RHIIndexBufferView::FromBuffer(
	const RHIIndexBuffer* buffer,
	uint32_t byteOffset,
	std::optional<uint32_t> sizeInBytes)
{
	RHIIndexBufferView view{};
	if (buffer == nullptr)
	{
		return view;
	}

	view.format = buffer->GetIndexFormat();
	if (!RHIBufferViewDetail::ResolveByteRange(
			buffer->GetGPUAddress(),
			buffer->GetSize(),
			RHIBufferViewDetail::IndexElementStride(view.format),
			byteOffset,
			sizeInBytes,
			view.gpuAddress,
			view.sizeInBytes))
	{
		return {};
	}

	return view;
}

inline RHIIndexBufferView RHIIndexBufferView::FromUpload(
	const RHIUploadAllocation& allocation,
	IndexFormat format,
	uint32_t byteOffset,
	std::optional<uint32_t> sizeInBytes)
{
	RHIIndexBufferView view{};
	view.format = format;
	if (allocation.cpuAddress == nullptr)
	{
		return {};
	}

	if (!RHIBufferViewDetail::ResolveByteRange(
			allocation.gpuVirtualAddress,
			allocation.size,
			RHIBufferViewDetail::IndexElementStride(format),
			byteOffset,
			sizeInBytes,
			view.gpuAddress,
			view.sizeInBytes))
	{
		return {};
	}

	return view;
}
