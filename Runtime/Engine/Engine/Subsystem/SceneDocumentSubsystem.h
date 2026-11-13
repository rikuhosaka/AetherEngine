#pragma once

#include "Runtime/Engine/SceneDocument/EditorSceneServices.h"
#include "Runtime/Engine/World/SceneContentServices.h"
#include "Runtime/Engine/Application/Subsystem/ISceneContent.h"
#include "Runtime/Engine/Application/Subsystem/ISubsystem.h"
#include "Runtime/Engine/AssetSystem/AssetCache.h"
#include "Runtime/Engine/SceneDocument/SceneDocument.h"

#include <filesystem>
#include <memory>

// 責任: 開いているシーン文書と、その CPU 側のアセット読み込みを持つ。
// 役割: 文書を編集用に公開する。読込結果は保留に置き、CommitPending まで公開しない。
//       メッシュとテクスチャの GPU アップロードは RenderSystem が行う。登録名は "SceneDocument"。
// 所有: SubsystemRegistry が unique_ptr で持つ。公開中の SceneDocument、保留中の SceneDocument、パス、AssetCache、EditorSceneServices をメンバとして所有する。
//       SceneContentServices は Engine の所有で、ここは非所有ポインタを借り、loader に自分を書く。
class SceneDocumentSubsystem final : public ISubsystem, public ISceneContent
{
public:
	explicit SceneDocumentSubsystem(
		std::filesystem::path assetsRoot = {},
		std::filesystem::path scenePath = {});

	[[nodiscard]] const char* GetName() const override { return "SceneDocument"; }

	[[nodiscard]] std::span<const char* const> GetDependencies() const override;

	[[nodiscard]] bool WantsTick() const override { return true; }

	Result<void> Initialize(SubsystemContext& ctx) override;
	void Tick(SubsystemContext& ctx, float deltaSeconds) override;
	Result<void> Load(World& world, SubsystemContext& ctx) override;
	void Shutdown(SubsystemContext& ctx) override;

	// 文書と CPU アセットを保留へ読む。公開中の文書は変えない。失敗時は保留も変えない。
	Result<void> ReadScene(const std::filesystem::path& scenePath);
	// 保留を公開中の文書にし、編集用サービスへ出す。保留が無ければ何もしない。
	void CommitPending();
	// 保留だけを捨てる。公開中の文書は変えない。
	void DiscardPending();
	[[nodiscard]] bool HasPending() const { return m_hasPending; }
	// HasPending のときだけ有効。
	[[nodiscard]] const SceneDocument& GetPendingDocument() const { return m_pending; }

	void SetDocument(SceneDocument document);
	void SetPath(std::filesystem::path path);
	[[nodiscard]] bool HasDocument() const { return m_hasDocument; }
	[[nodiscard]] const SceneDocument& GetDocument() const { return m_document; }
	[[nodiscard]] const AssetCache& GetAssetCache() const { return m_assetCache; }
	void Publish(EditorSceneServices& out) const;

private:
	std::filesystem::path m_assetsRoot{};
	std::filesystem::path m_scenePath{};
	AssetCache m_assetCache{};
	SceneDocument m_document{};
	std::filesystem::path m_path{};
	bool m_hasDocument = false;
	SceneDocument m_pending{};
	std::filesystem::path m_pendingPath{};
	bool m_hasPending = false;
	// Engine が所有するサービス。このサブシステムは借りて loader を書き込む。
	SceneContentServices* m_content = nullptr;
	// このサブシステムが所有する公開データ。Context に載るのはこのアドレスだけ。
	EditorSceneServices m_editorServices{};
};

std::unique_ptr<ISubsystem> CreateSceneDocumentSubsystem(
	std::filesystem::path assetsRoot,
	std::filesystem::path scenePath = {});
