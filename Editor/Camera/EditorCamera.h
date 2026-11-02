#pragma once

#include "Engine/Camera/CameraState.h"
#include "Engine/Camera/FreeFlyCameraController.h"

#include <cstdint>

class InputState;
struct EditorCameraServices;

// 責任: 編集ビューポート用の自由飛行カメラを更新する。
// 役割: 入力から姿勢を進め、渡された EditorCameraServices へ書き出す。
// 所有: SceneLoaderSubsystem のメンバ。姿勢とコントローラを値で持つ。公開先のサービスは所有しない。
class EditorCamera
{
public:
	void Reset();
	void Update(const InputState* input, float deltaSeconds, bool useMouse, bool useKeyboard);
	void Publish(uint32_t viewportWidth, uint32_t viewportHeight, EditorCameraServices& out) const;

private:
	static constexpr DirectX::XMFLOAT3 kStartPosition{ 0.0f, 1.6f, -4.0f };
	static constexpr DirectX::XMFLOAT3 kStartForward{ 0.0f, 0.0f, 1.0f };

	CameraState m_state{};
	FreeFlyCameraController m_controller{};
};
