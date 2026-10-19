#include "Editor/Camera/EditorCamera.h"

#include "Engine/Application/Services/EditorCameraServices.h"
#include "Engine/Camera/CameraExtract.h"
#include "Engine/Platform/InputState.h"

void EditorCamera::Reset()
{
	m_controller.Reset(m_state, kStartPosition, kStartForward);
}

void EditorCamera::Update(const InputState* input, float deltaSeconds, bool useMouse, bool useKeyboard)
{
	if (input == nullptr)
	{
		return;
	}

	m_controller.Update(m_state, *input, deltaSeconds, useMouse, useKeyboard);
}

void EditorCamera::Publish(uint32_t viewportWidth, uint32_t viewportHeight, EditorCameraServices& out) const
{
	out.valid = true;
	out.position = m_state.position;
	out.forward = m_state.forward;
	BuildExtractedView(m_state, viewportWidth, viewportHeight, out.view);
}
