#pragma once

#include "Engine/RHI/Common/RHIResource.h"

struct ID3D12Resource;

namespace DX12BarrierValidator
{
	struct TransitionContext
	{
		ID3D12Resource* resource = nullptr;
		ERHIResourceState currentState = ERHIResourceState::Common;
		ERHIResourceState newState = ERHIResourceState::Common;
		bool stateInitialized = false;
	};

	// Logs validation issues. Returns false if the transition must be skipped (e.g. null resource).
	bool ValidateTransition(const TransitionContext& context);

	void BeginFrame();
	void EndFrame();

} // namespace DX12BarrierValidator
