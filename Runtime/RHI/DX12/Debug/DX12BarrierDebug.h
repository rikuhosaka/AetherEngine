#pragma once

#include "Runtime/RHI/Interface/RHIBarrierDebug.h"

class DX12BarrierDebug final : public RHIBarrierDebug
{
public:
	void BeginFrame() override;
	void EndFrame() override;
};
