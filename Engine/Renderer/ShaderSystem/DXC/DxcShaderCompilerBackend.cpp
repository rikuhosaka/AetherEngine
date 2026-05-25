#include "Engine/Renderer/ShaderSystem/DXC/DxcShaderCompilerBackend.h"
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
		case ShaderStage::Vertex:
			return "vs";
		case ShaderStage::Pixel:
			return "ps";
		case ShaderStage::Compute:
			return "cs";
		case ShaderStage::Geometry:
			return "gs";
		case ShaderStage::Hull:
			return "hs";
		case ShaderStage::Domain:
			return "ds";
		default:
			return "vs";
		}
	}

	[[nodiscard]] const char* ModelSuffix(ShaderModel model)
	{
		switch (model)
		{
		case ShaderModel::SM6_0:
			return "6_0";
		case ShaderModel::SM6_1:
			return "6_1";
		case ShaderModel::SM6_2:
			return "6_2";
		case ShaderModel::SM6_3:
			return "6_3";
		case ShaderModel::SM6_4:
			return "6_4";
		case ShaderModel::SM6_5:
			return "6_5";
		case ShaderModel::SM6_6:
			return "6_6";
		case ShaderModel::SM6_7:
			return "6_7";
		default:
			return "6_6";
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
		void Add(std::wstring argument)
		{
			m_storage.push_back(std::move(argument));
			m_pointers.push_back(m_storage.back().c_str());
		}

		void AddUtf8(std::string_view argument)
		{
			Add(ToWide(std::string(argument)));
		}

		[[nodiscard]] LPCWSTR* Data() noexcept
		{
			return m_pointers.data();
		}

		[[nodiscard]] UINT32 Count() const noexcept
		{
			return static_cast<UINT32>(m_pointers.size());
		}

	private:
		std::vector<std::wstring> m_storage{};
		std::vector<LPCWSTR> m_pointers{};
	};
}

DxcShaderCompilerBackend::DxcShaderCompilerBackend(DxcShaderContext* context)
	: m_context(context)
{
}

ShaderCompileResult DxcShaderCompilerBackend::Compile(const ShaderCompileDesc& desc)
{
	ShaderCompileResult result{};

	if (m_context == nullptr || !m_context->IsInitialized())
	{
		result.Errors = "DXC shader context is not initialized.";
		return result;
	}

	DxcShaderImpl& impl = m_context->GetImpl();
	if (impl.utils == nullptr || impl.compiler == nullptr || impl.includeHandler == nullptr)
	{
		result.Errors = "DXC compiler backend is not initialized.";
		return result;
	}

	if (desc.FilePath.empty())
	{
		result.Errors = "Shader source path is empty.";
		return result;
	}

	ComPtr<IDxcBlobEncoding> sourceBlob{};
	const HRESULT loadHr = impl.utils->LoadFile(desc.FilePath.c_str(), nullptr, sourceBlob.GetAddressOf());
	if (FAILED(loadHr) || sourceBlob == nullptr)
	{
		result.Errors = "Failed to load shader source: " + desc.FilePath.string();
		return result;
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

	ComPtr<IDxcResult> compileResult{};
	const HRESULT compileHr = impl.compiler->Compile(
		&sourceBuffer,
		arguments.Data(),
		arguments.Count(),
		impl.includeHandler.Get(),
		IID_PPV_ARGS(compileResult.GetAddressOf()));

	if (FAILED(compileHr) || compileResult == nullptr)
	{
		result.Errors = "DXC Compile call failed.";
		return result;
	}

	ComPtr<IDxcBlobUtf8> errorsBlob{};
	if (SUCCEEDED(compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorsBlob.GetAddressOf()), nullptr))
		&& errorsBlob != nullptr)
	{
		const std::string diagnostics = BlobUtf8ToString(errorsBlob.Get());
		if (!diagnostics.empty())
		{
			result.Warnings = diagnostics;
		}
	}

	HRESULT status = E_FAIL;
	if (FAILED(compileResult->GetStatus(&status)))
	{
		result.Errors = result.Warnings.empty() ? "DXC compilation failed." : result.Warnings;
		result.Warnings.clear();
		return result;
	}

	if (FAILED(status))
	{
		result.Errors = result.Warnings.empty() ? "DXC compilation failed." : result.Warnings;
		result.Warnings.clear();
		return result;
	}

	ComPtr<IDxcBlob> shaderBlob{};
	if (FAILED(compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shaderBlob.GetAddressOf()), nullptr))
		|| shaderBlob == nullptr)
	{
		result.Errors = "DXC did not produce shader bytecode.";
		return result;
	}

	const std::byte* bytecodeBegin = static_cast<const std::byte*>(shaderBlob->GetBufferPointer());
	const std::size_t bytecodeSize = shaderBlob->GetBufferSize();
	result.Bytecode.Data.assign(bytecodeBegin, bytecodeBegin + bytecodeSize);
	result.Succeeded = !result.Bytecode.Data.empty();
	return result;
}
