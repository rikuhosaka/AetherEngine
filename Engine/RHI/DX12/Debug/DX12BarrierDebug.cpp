#include "Engine/RHI/DX12/Debug/DX12BarrierDebug.h"

#include "Engine/RHI/DX12/Debug/DX12BarrierValidator.h"

void DX12BarrierDebug::BeginFrame()
{
	DX12BarrierValidator::BeginFrame();
}

void DX12BarrierDebug::EndFrame()
{
	DX12BarrierValidator::EndFrame();
}
