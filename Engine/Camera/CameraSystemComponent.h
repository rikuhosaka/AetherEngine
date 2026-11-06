#pragma once

#include "Engine/Camera/CameraState.h"
#include "Engine/Camera/FreeFlyCameraController.h"
#include "Engine/Core/Log/Result.h"
#include "Engine/World/Component.h"

#include <DirectXMath.h>

class World;

// 責任: 付いているアクターの視点を自由飛行させる。
// 役割: 編集中の編集用 World と、再生中・一時停止中のゲーム用 World で、自分の Transform を入力で進める。
//       相対マウスが必要かを返す。レンズは同じアクターの CameraComponent が持つ。
// 所有: 付いている Actor がコンポーネントとして持つ。コントローラと作業用の姿勢はメンバ。
//       編集用 World は起動時に1つ作る。プレイ開始時は、写した先頭の CameraComponent へ足す。無ければ作る。
class CameraSystemComponent final : public Component
{
public:
	bool Tick(
		World& world,
		EntityId self,
		const SessionServices& session,
		const InputState* input,
		float deltaSeconds,
		bool mouseCaptured,
		bool keyboardCaptured) override;

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
// CameraComponent と CameraSystemComponent を足せなければエンティティも消す。
[[nodiscard]] EntityId SpawnCamera(
	World& world,
	const DirectX::XMFLOAT3* position,
	const DirectX::XMFLOAT3* forward);

// 先頭の CameraComponent に CameraSystemComponent が無ければ足す。
// カメラ自体が無ければ SpawnCamera で作る。足せなければ失敗する。
[[nodiscard]] Result<void> EnsurePlayCamera(
	World& world,
	const DirectX::XMFLOAT3* position,
	const DirectX::XMFLOAT3* forward);
