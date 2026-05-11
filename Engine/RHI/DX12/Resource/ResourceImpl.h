#pragma once
#include "Engine/RHI/Interface/RHIResource.h"


class ResourceImpl : public RHIResource
{
public:

	ResourceImpl() = default;
	~ResourceImpl() override = default;
	ComPtr<ID3D12Resource> resource;
};