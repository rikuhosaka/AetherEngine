#pragma once

#include "Engine/Application/Services/EditorSceneServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Subsystem/ISceneContent.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/AssetSystem/AssetCache.h"
#include "Engine/SceneDocument/SceneDocument.h"

#include <filesystem>
#include <memory>

// 責任: 開いているシーン文書と、そのアセット読み込みを持つ。
// 役割: 文書を編集用に公開し、ISceneContent として World へエンティティを生成する。登録名は "SceneDocument"。
// 所有: SubsystemRegistry が unique_ptr で持つ。SceneDocument、パス、AssetCache、EditorSceneServices をメンバとして所有する。
//       SceneContentServices は Engine の所有で、ここは非所有ポインタを借り、loader に自分を登録する。
class SceneDocumentSubsystem final : public ISubsystem, public ISceneContent
{
public:
	[[nodiscard]] const char* GetName() const override { return "SceneDocument"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	Result<void> Load(
		World& world,
		SubsystemContext& ctx,
		FrameContext& frameContext,
		RHICommandList* commandList) override;
	void Shutdown(SubsystemContext& ctx) override;

	void SetDocument(SceneDocument document);
	void SetPath(std::filesystem::path path);
	[[nodiscard]] const SceneDocument& GetDocument() const { return m_document; }
	void Publish(EditorSceneServices& out) const;

private:
	AssetCache m_assetCache{};
	SceneDocument m_document{};
	std::filesystem::path m_path{};
	bool m_hasDocument = false;
	// Engine が所有するサービス。このサブシステムは借りて loader を書き込む。
	SceneContentServices* m_content = nullptr;
	// このサブシステムが所有する公開データ。Context に載るのはこのアドレスだけ。
	EditorSceneServices m_editorServices{};
};

std::unique_ptr<ISubsystem> CreateSceneDocumentSubsystem();
