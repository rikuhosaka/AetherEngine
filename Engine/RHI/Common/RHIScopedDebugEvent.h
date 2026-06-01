#pragma once

class RHICommandList;

class RHIScopedDebugEvent
{
public:
	RHIScopedDebugEvent(RHICommandList* commandList, const char* name);
	~RHIScopedDebugEvent();

	RHIScopedDebugEvent(const RHIScopedDebugEvent&) = delete;
	RHIScopedDebugEvent& operator=(const RHIScopedDebugEvent&) = delete;

private:
	RHICommandList* m_commandList = nullptr;
};

inline RHIScopedDebugEvent::RHIScopedDebugEvent(RHICommandList* commandList, const char* name)
	: m_commandList(commandList)
{
	if (m_commandList != nullptr)
	{
		m_commandList->BeginDebugEvent(name);
	}
}

inline RHIScopedDebugEvent::~RHIScopedDebugEvent()
{
	if (m_commandList != nullptr)
	{
		m_commandList->EndDebugEvent();
	}
}
