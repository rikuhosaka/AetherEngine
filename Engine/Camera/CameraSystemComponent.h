#pragma once

#include "Engine/Camera/CameraState.h"
#include "Engine/Camera/FreeFlyCameraController.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/World/Component.h"

#include <DirectXMath.h>

class InputState;
class World;

// 責任: 付いているアクターの視点を自由飛行させる。
// 役割: PlayerController が渡した入力で自分の Transform を進める。Tick 自体は入力を見ない。
//       レンズは同じアクターの CameraComponent が持つ。
// 所有: 付いている Actor がコンポーネントとして持つ。コントローラと作業用の姿勢はメンバ。
//       編集用 World は起動時に1つ作る。プレイ開始時は、写した先頭の CameraComponent へ足す。無ければ作る。
class CameraSystemComponent final : public Component
{
public:
	// useMouse / useKeyboard が false の軸は進めない。input が無ければ何もしない。
	void ApplyControl(
		World& world,
		const InputState* input,
		float deltaSeconds,
		bool useMouse,
		bool useKeyboard);

private:
	// 最初の入力まで false。そのときアクターの姿勢からヨーとピッチを取り直す。
	bool m_poseReady = false;
	FreeFlyCameraController m_cameraController{};
	CameraState m_cameraPose{};
};

// 生存していて CameraSystemComponent を持つ先頭エンティティ。無ければ無効。
[[nodiscard]] EntityId FindCamera(const World& world);

// 親の無いカメラを1つ足す。位置と前方が両方非 null のときだけその姿勢を使う。
// 片方でも null なら両方をプレイ用の代替姿勢にする。
// CameraComponent、CameraSystemComponent、PlayerController を足せなければエンティティも消す。
[[nodiscard]] EntityId SpawnCamera(
	World& world,
	const DirectX::XMFLOAT3* position,
	const DirectX::XMFLOAT3* forward);

// 先頭の CameraComponent に CameraSystemComponent と PlayerController が無ければ足す。
// カメラ自体が無ければ SpawnCamera で作る。足せなければ失敗する。
[[nodiscard]] Result<void> EnsurePlayCamera(
	World& world,
	const DirectX::XMFLOAT3* position,
	const DirectX::XMFLOAT3* forward);
