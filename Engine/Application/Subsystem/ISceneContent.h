#pragma once

#include "Engine/Core/Log/Result.h"

class SubsystemContext;
class World;

// 責任: シーン内容を World の文書へ読み込む処理を定義する。
// 役割: SceneContentServices::loader に入り、World::LoadContent から呼ばれる。
// 所有: 実装を持つサブシステムが自分自身をこの型として公開する。サービスは実装を所有しない。
// GPU へのアップロードはここには含めない。
class ISceneContent
{
public:
	virtual ~ISceneContent() = default;

	virtual Result<void> Load(World& world, SubsystemContext& ctx) = 0;
};
