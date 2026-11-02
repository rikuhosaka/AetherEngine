#pragma once

#include "Engine/Application/Subsystem/IWorldSubsystem.h"
#include "Engine/Camera/CameraState.h"
#include "Engine/Camera/FreeFlyCameraController.h"
#include "Engine/World/EntityId.h"

class InputState;
class World;

// 責任: プレイ中の視点を動かす。
// 役割: 先頭のカメラアクターを自由飛行させ、相対マウスが必要かを返す。無ければ開始時に1つ生成する。
// 所有: World が名前 "Camera" で持つ。コントローラと作業用の姿勢はメンバ。
//       レンズ、変換、エンティティ自体は World の文書が所有する。m_cameraEntity は参照用の ID。
class CameraSubsystem final : public IWorldSubsystem
{
public:
	[[nodiscard]] const char* GetName() const override { return "Camera"; }

	[[nodiscard]] SubsystemTickGroup GetTickGroup() const override
	{
		return SubsystemTickGroup::Game;
	}

	Result<void> OnBeginPlay(
		World& world,
		const DirectX::XMFLOAT3* cameraPosition,
		const DirectX::XMFLOAT3* cameraForward) override;
	void OnEndPlay(World& world) override;
	bool Tick(
		World& world,
		const SessionServices& session,
		const InputState* input,
		float deltaSeconds,
		bool mouseCaptured,
		bool keyboardCaptured) override;

	// 生存していて CameraComponent を持つ先頭エンティティ。無ければ無効。
	[[nodiscard]] EntityId FindCamera(const World& world) const;

	// 親の無いカメラを1つ足す。位置と前方が両方非 null のときだけその姿勢を使う。
	// 片方でも null なら両方をプレイ用の代替姿勢にする。コンポーネントを足せなければエンティティも消す。
	[[nodiscard]] EntityId SpawnCamera(
		World& world,
		const DirectX::XMFLOAT3* position,
		const DirectX::XMFLOAT3* forward);

	// 自由飛行の結果をカメラのローカル変換へ書く。
	// 戻り値は、そのカメラが相対マウスを要求しているか。UI がマウスを取っている間は false。
	[[nodiscard]] bool UpdateCamera(
		World& world,
		EntityId camera,
		const InputState* input,
		float deltaSeconds,
		bool mouseCaptured,
		bool keyboardCaptured);

private:
	// このサブシステムが所有する操作状態。コピー先は空のコントローラから始まる。
	FreeFlyCameraController m_cameraController{};
	CameraState m_cameraPose{};
	EntityId m_cameraEntity{};
};
