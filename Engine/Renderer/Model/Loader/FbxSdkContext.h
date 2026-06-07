#pragma once

#include "Engine/Core/Log/Result.h"

#include <fbxsdk/core/fbxmanager.h>
#include <fbxsdk/fileio/fbxiosettings.h>

class FbxSdkContext
{
public:
	FbxSdkContext() = default;

	FbxSdkContext(const FbxSdkContext&) = delete;
	FbxSdkContext& operator=(const FbxSdkContext&) = delete;

	FbxSdkContext(FbxSdkContext&& other) noexcept;
	FbxSdkContext& operator=(FbxSdkContext&& other) noexcept;

	~FbxSdkContext();

	static Result<FbxSdkContext> Create();

	[[nodiscard]] fbxsdk::FbxManager* GetManager() const noexcept { return m_manager; }
	[[nodiscard]] fbxsdk::FbxIOSettings* GetIOSettings() const noexcept { return m_ioSettings; }

private:
	fbxsdk::FbxManager* m_manager = nullptr;
	fbxsdk::FbxIOSettings* m_ioSettings = nullptr;
};
