#include "Engine/RHI/DX12/pch.h"
#include "Engine/RHI/DX12/Debug/DX12BarrierValidator.h"

#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"

#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG

#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/LogCommon.h"
#include "Engine/Core/Log/LogMacros.h"

#include <format>
#include <string>
#include <vector>

namespace
{

uint32_t g_warningCount = 0;
uint32_t g_errorCount = 0;

const char* ToString(ERHIResourceState state)
{
	switch (state)
	{
	case ERHIResourceState::Common:
		return "Common";
	case ERHIResourceState::CopyDest:
		return "CopyDest";
	case ERHIResourceState::CopySource:
		return "CopySource";
	case ERHIResourceState::VertexAndConstantBuffer:
		return "VertexAndConstantBuffer";
	case ERHIResourceState::IndexBuffer:
		return "IndexBuffer";
	case ERHIResourceState::RenderTarget:
		return "RenderTarget";
	case ERHIResourceState::UnorderedAccess:
		return "UnorderedAccess";
	case ERHIResourceState::DepthWrite:
		return "DepthWrite";
	case ERHIResourceState::DepthRead:
		return "DepthRead";
	case ERHIResourceState::NonPixelShaderResource:
		return "NonPixelShaderResource";
	case ERHIResourceState::PixelShaderResource:
		return "PixelShaderResource";
	case ERHIResourceState::Present:
		return "Present";
	default:
		return "Unknown";
	}
}

std::string WideToUtf8(const wchar_t* wide)
{
	if (wide == nullptr || wide[0] == L'\0')
	{
		return {};
	}

	const int size = WideCharToMultiByte(
		CP_UTF8,
		0,
		wide,
		-1,
		nullptr,
		0,
		nullptr,
		nullptr);
	if (size <= 1)
	{
		return {};
	}

	std::string utf8(static_cast<size_t>(size - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), size, nullptr, nullptr);
	return utf8;
}

std::string GetResourceLabel(ID3D12Resource* resource)
{
	if (resource == nullptr)
	{
		return "null";
	}

	ComPtr<ID3D12Object> object;
	if (SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&object))))
	{
		UINT nameSize = 0;
		if (SUCCEEDED(object->GetPrivateData(WKPDID_D3DDebugObjectName, &nameSize, nullptr)) && nameSize > sizeof(wchar_t))
		{
			std::vector<std::byte> nameBytes(nameSize);
			if (SUCCEEDED(object->GetPrivateData(
				WKPDID_D3DDebugObjectName,
				&nameSize,
				nameBytes.data())))
			{
				const wchar_t* wideName = reinterpret_cast<const wchar_t*>(nameBytes.data());
				const std::string utf8 = WideToUtf8(wideName);
				if (!utf8.empty())
				{
					return utf8;
				}
			}
		}
	}

	return std::format("0x{:p}", static_cast<void*>(resource));
}

void LogBarrierIssue(
	LogLevel level,
	const DX12BarrierValidator::TransitionContext& context,
	std::string_view message)
{
	const std::string resourceLabel = GetResourceLabel(context.resource);
	const std::string fullMessage = std::format(
		"[Barrier] {} ({} -> {}) on '{}'",
		message,
		ToString(context.currentState),
		ToString(context.newState),
		resourceLabel);

	Logger::Instance().Write(
		LogCategory::RHI,
		level,
		fullMessage,
		std::source_location::current());

	if (level == LogLevel::Warning)
	{
		++g_warningCount;
	}
	else if (level == LogLevel::Error || level == LogLevel::Fatal)
	{
		++g_errorCount;
	}
}

bool IsSuspiciousPresentTransition(ERHIResourceState currentState, ERHIResourceState newState)
{
	return newState == ERHIResourceState::Present
		&& currentState != ERHIResourceState::RenderTarget;
}

bool IsSuspiciousRenderTargetTransition(ERHIResourceState currentState, ERHIResourceState newState)
{
	return newState == ERHIResourceState::RenderTarget
		&& currentState != ERHIResourceState::Present
		&& currentState != ERHIResourceState::Common
		&& currentState != ERHIResourceState::CopyDest;
}

} // namespace

namespace DX12BarrierValidator
{

bool ValidateTransition(const TransitionContext& context)
{
	if (context.resource == nullptr)
	{
		LogBarrierIssue(
			LogLevel::Error,
			context,
			"Transition requested on a null resource");
		return false;
	}

	if (!context.stateInitialized)
	{
		LogBarrierIssue(
			LogLevel::Error,
			context,
			"Transition requested before initial resource state was set");
		return false;
	}

	if (context.currentState == context.newState)
	{
		LogBarrierIssue(
			LogLevel::Warning,
			context,
			"Redundant resource barrier");
		return false;
	}

	if (IsSuspiciousPresentTransition(context.currentState, context.newState))
	{
		LogBarrierIssue(
			LogLevel::Warning,
			context,
			"Transition to Present is usually expected from RenderTarget");
	}

	if (IsSuspiciousRenderTargetTransition(context.currentState, context.newState))
	{
		LogBarrierIssue(
			LogLevel::Warning,
			context,
			"Unexpected transition to RenderTarget");
	}

	return true;
}

void BeginFrame()
{
	g_warningCount = 0;
	g_errorCount = 0;
}

void EndFrame()
{
	if (!DX12DebugSettingsData::Get().enableFrameBarrierReport)
	{
		return;
	}

	if (g_warningCount == 0 && g_errorCount == 0)
	{
		return;
	}

	LOG_WARN(
		LogCategory::RHI,
		std::format(
			"[Barrier] Frame summary: {} warning(s), {} error(s)",
			g_warningCount,
			g_errorCount));
}

} // namespace DX12BarrierValidator

#else

namespace DX12BarrierValidator
{

bool ValidateTransition(const TransitionContext& context)
{
	(void)context;
	return context.resource != nullptr;
}

void BeginFrame() {}

void EndFrame() {}

} // namespace DX12BarrierValidator

#endif
