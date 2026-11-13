#pragma once

#include "Runtime/Engine/World/Component.h"

class World;

// 責任: 操作対象のアクターへ、そのフレームの入力を渡す。
// 役割: InputSubsystem から入力を読み、同じアクターの CameraSystemComponent へ渡す。
//       UI が取っている軸は渡さない。相対マウスが必要なら InputServices へ直接書く。
//       編集中は編集用 World、再生中と一時停止中はゲーム用 World だけが Tick される。
//       このコンポーネントは、Tick された World のカメラを動かす。
// 所有: 付いている Actor がコンポーネントとして持つ。入力状態は所有しない。
//       SpawnCamera と EnsurePlayCamera がカメラアクターへ足す。
class PlayerController final : public Component
{
public:
	void Tick(World& world, float deltaSeconds) override;
};
