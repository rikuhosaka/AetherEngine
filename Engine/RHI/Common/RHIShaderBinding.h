#pragma once


enum class RHIShaderBinding : uint8_t
{
	DrawInfo,
    CbvViewProj,
    SrvWorldMat,
    SrvBones,
    SrvMaterial,
    SrvTextures,
};

enum class RHISamplerBinding : uint8_t
{
	Linear,
	Aniso,
	Wrap,
};