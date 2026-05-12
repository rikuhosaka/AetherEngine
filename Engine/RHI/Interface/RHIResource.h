#pragma once
#include "Engine/RHI/Interface/RHICommon.h"


class RHICommandList;

class RHIResource
{
public:
	virtual ~RHIResource() = default;

	virtual void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) = 0;
	ERHIResourceState GetState() const { return m_currentState; }

protected:
	RHIResource() = default;
	ERHIResourceState m_currentState = ERHIResourceState::Common;
};