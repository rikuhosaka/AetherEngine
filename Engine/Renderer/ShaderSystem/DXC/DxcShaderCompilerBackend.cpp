#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderCompilerBackend.h"

#include "Engine/Core/Log/LogMacros.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderContext.h"
#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderImpl.h"

#include <dxcapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
[[nodiscard]] std::wstring ToWide(const std::string& text)
{
	if (text.empty())
	{
		return {};
	}

	const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
	if (requiredSize <= 0)
	{
		return {};
	}

	std::wstring wide(static_cast<std::size_t>(requiredSize - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), requiredSize);
	return wide;
}

[[nodiscard]] const char* StagePrefix(ShaderStage stage)
{
	switch (stage)
	{
	case ShaderStage::Vertex: return "vs";
	case ShaderStage::Pixel: return "ps";
	case ShaderStage::Compute: return "cs";
	case ShaderStage::Geometry: return "gs";
	case ShaderStage::Hull: return "hs";
	case ShaderStage::Domain: return "ds";
	default: return "vs";
	}
}

[[nodiscard]] const char* ModelSuffix(ShaderModel model)
{
	switch (model)
	{
	case ShaderModel::SM6_0: return "6_0";
	case ShaderModel::SM6_1: return "6_1";
	case ShaderModel::SM6_2: return "6_2";
	case ShaderModel::SM6_3: return "6_3";
	case ShaderModel::SM6_4: return "6_4";
	case ShaderModel::SM6_5: return "6_5";
	case ShaderModel::SM6_6: return "6_6";
	case ShaderModel::SM6_7: return "6_7";
	default: return "6_6";
	}
}

[[nodiscard]] std::string BuildTargetProfile(const ShaderCompileDesc& desc)
{
	return std::string(StagePrefix(desc.Stage)) + "_" + ModelSuffix(desc.Model);
}

[[nodiscard]] std::string BlobUtf8ToString(IDxcBlobUtf8* blob)
{
	if (blob == nullptr || blob->GetStringLength() == 0)
	{
		return {};
	}

	return std::string(blob->GetStringPointer(), blob->GetStringLength());
}

class DxcArguments
{
public:

	void Add(std::wstring arg)
	{
		m_storage.push_back(std::move(arg));
	}

	void AddUtf8(std::string_view arg)
	{
		Add(ToWide(std::string(arg)));
	}

	LPCWSTR* Data()
	{
		m_pointers.clear();

		for (auto& s : m_storage)
		{
			m_pointers.push_back(s.c_str());
		}

		return m_pointers.data();
	}
	UINT32 Count() const
	{
		return static_cast<UINT32>(m_storage.size());
	}

private:

	std::vector<std::wstring> m_storage;
	std::vector<LPCWSTR> m_pointers;
};
} // namespace

DxcShaderCompilerBackend::DxcShaderCompilerBackend(DxcShaderContext* context)
	: m_context(context)
{
}

Result<ShaderBytecode> DxcShaderCompilerBackend::Compile(const ShaderCompileDesc& desc)
{
	if (m_context == nullptr || !m_context->IsInitialized())
	{
		return FailInternal<ShaderBytecode>(
			LogCategory::Renderer,
			ErrorCode::ShaderCompileFailed,
			"DXC shader context is not initialized.");
	}

	DxcShaderImpl& impl = m_context->GetImpl();
	if (impl.utils == nullptr || impl.compiler == nullptr || impl.includeHandler == nullptr)
	{
		return FailInternal<ShaderBytecode>(
			LogCategory::Renderer,
			ErrorCode::ShaderCompileFailed,
			"DXC compiler backend is not initialized.");
	}

	if (desc.FilePath.empty())
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::InvalidArgument,
			"Shader source path is empty.");
	}

	ComPtr<IDxcBlobEncoding> sourceBlob{};
	const HRESULT loadHr = impl.utils->LoadFile(desc.FilePath.c_str(), nullptr, sourceBlob.GetAddressOf());
	if (FAILED(loadHr) || sourceBlob == nullptr)
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::FileNotFound,
			"Failed to load shader source: " + desc.FilePath.string());
	}

	DxcBuffer sourceBuffer{};
	sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
	sourceBuffer.Size = sourceBlob->GetBufferSize();
	sourceBuffer.Encoding = DXC_CP_UTF8;

	DxcArguments arguments{};
	arguments.Add(L"-E");
	arguments.AddUtf8(desc.EntryPoint);
	arguments.Add(L"-T");
	arguments.AddUtf8(BuildTargetProfile(desc));

	for (const std::filesystem::path& includeDirectory : desc.IncludeDirectories)
	{
		arguments.Add(L"-I");
		arguments.Add(includeDirectory.wstring());
	}

	for (const ShaderDefine& define : desc.Defines)
	{
		arguments.Add(L"-D");
		arguments.AddUtf8(define.Name + "=" + define.Value);
	}

	if (desc.Debug)
	{
		arguments.Add(L"-Zi");
		arguments.Add(L"-Qembed_debug");
	}

	if (!desc.Optimization)
	{
		arguments.Add(L"-O0");
	}

	if (desc.TreatWarningsAsErrors)
	{
		arguments.Add(L"-WX");
	}

	auto profile = BuildTargetProfile(desc);

	LOG_INFO(
		LogCategory::Renderer,
		"Profile=[" + profile + "] Length=" +
		std::to_string(profile.size()));

	ComPtr<IDxcResult> compileResult{};
	const HRESULT compileHr = impl.compiler->Compile(
		&sourceBuffer,
		arguments.Data(),
		arguments.Count(),
		impl.includeHandler.Get(),
		IID_PPV_ARGS(compileResult.GetAddressOf()));

	if (FAILED(compileHr) || compileResult == nullptr)
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"DXC Compile call failed.");
	}

	std::string diagnostics;
	ComPtr<IDxcBlobUtf8> errorsBlob{};
	if (SUCCEEDED(compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorsBlob.GetAddressOf()), nullptr))
		&& errorsBlob != nullptr)
	{
		diagnostics = BlobUtf8ToString(errorsBlob.Get());
	}

	HRESULT status = E_FAIL;
	if (FAILED(compileResult->GetStatus(&status)))
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			diagnostics.empty() ? "DXC compilation failed." : diagnostics);
	}

	if (FAILED(status))
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			diagnostics.empty() ? "DXC compilation failed." : diagnostics);
	}

	ComPtr<IDxcBlob> shaderBlob{};
	if (FAILED(compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shaderBlob.GetAddressOf()), nullptr))
		|| shaderBlob == nullptr)
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"DXC did not produce shader bytecode.");
	}

	ShaderBytecode bytecode{};
	const std::byte* bytecodeBegin = static_cast<const std::byte*>(shaderBlob->GetBufferPointer());
	const std::size_t bytecodeSize = shaderBlob->GetBufferSize();
	bytecode.Data.assign(bytecodeBegin, bytecodeBegin + bytecodeSize);
	if (bytecode.Data.empty())
	{
		return FailRuntime<ShaderBytecode>(
			LogCategory::Asset,
			ErrorCode::ShaderCompileFailed,
			"DXC produced empty shader bytecode.");
	}

	if (!diagnostics.empty())
	{
		LOG_WARN(LogCategory::Renderer, diagnostics);
	}

	return MakeOk(std::move(bytecode));
}
