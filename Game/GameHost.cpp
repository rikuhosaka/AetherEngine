#include "Game/GameHost.h"

#include "Engine/Application/Services/InputServices.h"
#include "Engine/Application/Services/RenderServices.h"
#include "Engine/Application/Services/WindowServices.h"
#include "Engine/Application/Subsystem/SubsystemContext.h"
#include "Engine/Platform/InputState.h"

GameHost::GameHost(SubsystemContext& context)
	: m_context(context)
{
}

SubsystemContext& GameHost::GetContext()
{
	return m_context;
}

const InputState* GameHost::GetInput()
{
	const InputServices* inputServices = m_context.GetService<InputServices>();
	if (inputServices == nullptr)
	{
		return nullptr;
	}

	return inputServices->state;
}

void GameHost::SetRelativeMouse(bool enabled)
{
	InputServices* inputServices = m_context.GetService<InputServices>();
	if (inputServices == nullptr)
	{
		return;
	}

	inputServices->relativeMouse = enabled;
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
