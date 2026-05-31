#include "Game/GameHost.h"

#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Platform/InputManager.h"

GameHost::GameHost(SubsystemContext& context)
	: m_context(context)
{
}

SubsystemContext& GameHost::GetContext()
{
	return m_context;
}

InputManager& GameHost::GetInput()
{
	auto* inputServices = m_context.GetService<InputServices>();
	if (inputServices == nullptr || inputServices->input == nullptr)
	{
		return InputManager::Get();
	}

	return *inputServices->input;
}

float GameHost::GetDeltaSeconds() const
{
	return m_context.GetDeltaSeconds();
}
