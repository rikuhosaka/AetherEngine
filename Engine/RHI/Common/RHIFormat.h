#pragma once

// NOTE:
// - RTV_FORMAT / DSV_FORMAT are used by legacy pipeline descriptions.
// - ERHIFormat is used by texture resources and SRV/CBV descriptors.

enum class RTV_FORMAT
{
	R8G8B8A8_UNORM,
	R16G16B16A16_FLOAT,
	R32G32B32A32_FLOAT
};

enum class DSV_FORMAT
{
	D24_UNORM_S8_UINT,
	D32_FLOAT,
	Unknown
};

enum class ERHIFormat
{
	Unknown,

	R8G8B8A8_UNORM,
	R8G8B8A8_UNORM_SRGB,
	B8G8R8A8_UNORM,
	R16G16B16A16_FLOAT,
	R32G32B32A32_FLOAT,
	R32_TYPELESS,
	R32_FLOAT,
	D24_UNORM_S8_UINT,
	D32_FLOAT
};
