#pragma once

#include "Engine/Application/Services/EditorSceneServices.h"
#include "Engine/Application/Services/SceneContentServices.h"
#include "Engine/Application/Subsystem/ISceneContent.h"
#include "Engine/Application/Subsystem/ISubsystem.h"
#include "Engine/AssetSystem/AssetCache.h"
#include "Engine/SceneDocument/SceneDocument.h"

#include <filesystem>
#include <memory>

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
	SceneContentServices* m_content = nullptr;
	EditorSceneServices m_editorServices{};
};

std::unique_ptr<ISubsystem> CreateSceneDocumentSubsystem();
