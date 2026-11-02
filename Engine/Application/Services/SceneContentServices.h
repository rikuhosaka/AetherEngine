#pragma once

#include "Engine/Application/Subsystem/ISceneContent.h"

// 責任: シーンを World へ読み込む口と、読み込み済みかを共有する。
// 役割: BeginPlay が loaded を見て、セッションを開いてよいかを判断する。
// 所有: Engine のメンバ。loader は非所有で、SceneDocumentSubsystem が自分自身を登録する。
struct SceneContentServices
{
	ISceneContent* loader = nullptr;
	bool loaded = false;
};
