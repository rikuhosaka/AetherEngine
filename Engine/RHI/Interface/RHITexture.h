#pragma once
#include "Engine/RHI/Interface/RHIResource.h"

class RHITexture : public RHIResource
{
public:
	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;
	virtual uint32_t GetMipLevels() const = 0;
	virtual uint32_t GetArraySize() const = 0;
	virtual uint32_t GetSampleCount() const = 0;
	virtual uint32_t GetSampleQuality() const = 0;
protected:
	RHITexture() = default;
};