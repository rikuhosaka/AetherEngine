#pragma once

#include <filesystem>

struct SceneDocument;

// 責任: 開いているシーン文書を編集 UI から参照できるようにする。
// 役割: 文書とファイルパスを公開する。文書の中身をここでは変更しない。
// 所有: SceneDocumentSubsystem のメンバ。document は同じサブシステムが持つ SceneDocument を指す非所有ポインタ。
struct EditorSceneServices
{
	const SceneDocument* document = nullptr;
	std::filesystem::path path{};
};
