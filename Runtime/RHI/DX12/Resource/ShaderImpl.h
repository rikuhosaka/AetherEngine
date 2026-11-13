#pragma once


class ShaderImpl
{
public:
	virtual ~ShaderImpl() = default;
	ComPtr<ID3DBlob> blob;
};