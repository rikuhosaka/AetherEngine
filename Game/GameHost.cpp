#include "Game/GameHost.h"

#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Services/WindowServices.h"
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

RenderServices* GameHost::GetRenderServices()
{
	return m_context.GetService<RenderServices>();
}

std::filesystem::path GameHost::GetShaderRoot() const
{
	const RenderServices* renderServices = m_context.GetService<RenderServices>();
	if (renderServices == nullptr)
	{
		return {};
	}

	return renderServices->shaderRoot;
}

std::pair<uint32_t, uint32_t> GameHost::GetViewportSize() const
{
	const WindowServices* windowServices = m_context.GetService<WindowServices>();
	if (windowServices == nullptr)
	{
		return { 0, 0 };
	}

	return { windowServices->clientWidth, windowServices->clientHeight };
}
