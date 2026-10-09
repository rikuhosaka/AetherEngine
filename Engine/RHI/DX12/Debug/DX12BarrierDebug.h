#pragma once

#include "Engine/RHI/Interface/RHIBarrierDebug.h"

class DX12BarrierDebug final : public RHIBarrierDebug
{
public:
	void BeginFrame() override;
	void EndFrame() override;
};
