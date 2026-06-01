#pragma once
#include "Engine/RHI/Common/RHIResource.h"


class RHICommandList;

class RHIResource
{
public:
	virtual ~RHIResource() = default;

	virtual void TransitionResource(ERHIResourceState newState, const RHICommandList* commandList) = 0;
	ERHIResourceState GetState() const { return m_currentState; }

	// GPU 使用前のみ。スワップチェーン等の初期状態設定用。
	void SetInitialResourceState(ERHIResourceState state)
	{
		m_currentState = state;
		MarkStateInitialized();
	}

	[[nodiscard]] bool IsStateInitialized() const { return m_stateInitialized; }

protected:
	RHIResource() = default;

	void MarkStateInitialized() { m_stateInitialized = true; }

	ERHIResourceState m_currentState = ERHIResourceState::Common;
	bool m_stateInitialized = false;
};