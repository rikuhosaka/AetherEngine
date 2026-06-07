#include "Engine/Renderer/Model/Loader/FbxSdkContext.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Core/Log/Result.h"

#include <fbxsdk.h>
#include <fbxsdk/fileio/fbxiosettingspath.h>

FbxSdkContext::FbxSdkContext(FbxSdkContext&& other) noexcept
	: m_manager(other.m_manager)
	, m_ioSettings(other.m_ioSettings)
{
	other.m_manager = nullptr;
	other.m_ioSettings = nullptr;
}

FbxSdkContext& FbxSdkContext::operator=(FbxSdkContext&& other) noexcept
{
	if (this != &other)
	{
		if (m_manager != nullptr)
		{
			m_manager->Destroy();
		}

		m_manager = other.m_manager;
		m_ioSettings = other.m_ioSettings;
		other.m_manager = nullptr;
		other.m_ioSettings = nullptr;
	}

	return *this;
}

FbxSdkContext::~FbxSdkContext()
{
	if (m_manager != nullptr)
	{
		m_manager->Destroy();
		m_manager = nullptr;
		m_ioSettings = nullptr;
	}
}

Result<FbxSdkContext> FbxSdkContext::Create()
{
	fbxsdk::FbxManager* manager = fbxsdk::FbxManager::Create();
	if (manager == nullptr)
	{
		return FailRuntime<FbxSdkContext>(
			LogCategory::Asset,
			ErrorCode::OutOfMemory,
			"Failed to create FBX SDK manager");
	}

	fbxsdk::FbxIOSettings* ioSettings = fbxsdk::FbxIOSettings::Create(manager, IOSROOT);
	if (ioSettings == nullptr)
	{
		manager->Destroy();
		return FailRuntime<FbxSdkContext>(
			LogCategory::Asset,
			ErrorCode::OutOfMemory,
			"Failed to create FBX IO settings");
	}

	manager->SetIOSettings(ioSettings);
	ioSettings->SetBoolProp(IMP_FBX_MATERIAL, true);
	ioSettings->SetBoolProp(IMP_FBX_TEXTURE, true);
	ioSettings->SetBoolProp(IMP_GEOMETRY, true);
	ioSettings->SetBoolProp(IMP_FBX_ANIMATION, false);
	ioSettings->SetBoolProp(IMP_FBX_GOBO, false);

	FbxSdkContext context{};
	context.m_manager = manager;
	context.m_ioSettings = ioSettings;
	return MakeOk(std::move(context));
}
