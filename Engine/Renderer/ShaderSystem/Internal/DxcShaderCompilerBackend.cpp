#include "Engine/Renderer/ShaderSystem/Internal/DxcShaderCompilerBackend.h"

#include <fstream>
#include <sstream>

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

	[[nodiscard]] std::string QuoteArgument(const std::string& argument)
	{
		std::string quoted = "\"";
		for (const char ch : argument)
		{
			if (ch == '"')
			{
				quoted += "\\\"";
			}
			else
			{
				quoted += ch;
			}
		}
		quoted += '"';
		return quoted;
	}

	[[nodiscard]] bool ReadFileBytes(const std::filesystem::path& path, std::vector<std::byte>& outBytes)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			return false;
		}

		stream.seekg(0, std::ios::end);
		const std::streamoff size = stream.tellg();
		if (size < 0)
		{
			return false;
		}

		outBytes.resize(static_cast<std::size_t>(size));
		stream.seekg(0, std::ios::beg);
		stream.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(outBytes.size()));
		return static_cast<bool>(stream);
	}

	[[nodiscard]] bool RunProcess(
		const std::wstring& commandLine,
		std::string& capturedOutput,
		DWORD& exitCode)
	{
		SECURITY_ATTRIBUTES securityAttributes{};
		securityAttributes.nLength = sizeof(securityAttributes);
		securityAttributes.bInheritHandle = TRUE;

		HANDLE stdoutRead = nullptr;
		HANDLE stdoutWrite = nullptr;
		if (!CreatePipe(&stdoutRead, &stdoutWrite, &securityAttributes, 0))
		{
			return false;
		}

		SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		startupInfo.dwFlags = STARTF_USESTDHANDLES;
		startupInfo.hStdOutput = stdoutWrite;
		startupInfo.hStdError = stdoutWrite;

		PROCESS_INFORMATION processInfo{};
		std::wstring mutableCommandLine = commandLine;
		const BOOL created = CreateProcessW(
			nullptr,
			mutableCommandLine.data(),
			nullptr,
			nullptr,
			TRUE,
			CREATE_NO_WINDOW,
			nullptr,
			nullptr,
			&startupInfo,
			&processInfo);
		CloseHandle(stdoutWrite);

		if (!created)
		{
			CloseHandle(stdoutRead);
			return false;
		}

		char buffer[4096]{};
		DWORD bytesRead = 0;
		while (ReadFile(stdoutRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
		{
			capturedOutput.append(buffer, buffer + bytesRead);
		}

		WaitForSingleObject(processInfo.hProcess, INFINITE);
		GetExitCodeProcess(processInfo.hProcess, &exitCode);

		CloseHandle(stdoutRead);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return true;
	}
}

DxcShaderCompilerBackend::DxcShaderCompilerBackend(
	std::filesystem::path dxcExecutable,
	std::filesystem::path scratchDirectory)
	: m_dxcExecutable(std::move(dxcExecutable))
	, m_scratchDirectory(std::move(scratchDirectory))
{
	std::error_code errorCode{};
	std::filesystem::create_directories(m_scratchDirectory, errorCode);
}

bool DxcShaderCompilerBackend::Compile(const ShaderCompileJob& job, ShaderCompileOutput& out)
{
	out.Bytecode.clear();
	out.Diagnostics = {};

	if (m_dxcExecutable.empty() || !std::filesystem::exists(m_dxcExecutable))
	{
		out.Diagnostics.PrimaryErrorMessage = "DXC executable was not found.";
		return false;
	}

	const std::filesystem::path outputPath =
		m_scratchDirectory / (job.EntryPoint + "_" + job.Profile + ".cso");

	std::ostringstream commandLine;
	commandLine << QuoteArgument(m_dxcExecutable.string());
	commandLine << " -T " << QuoteArgument(job.Profile);
	commandLine << " -E " << QuoteArgument(job.EntryPoint);
	commandLine << " -Fo " << QuoteArgument(outputPath.string());

	for (const std::string& includePath : job.IncludeSearchPaths)
	{
		commandLine << " -I " << QuoteArgument(includePath);
	}

	for (const auto& [name, value] : job.Defines)
	{
		commandLine << " -D " << QuoteArgument(name + "=" + value);
	}

	commandLine << ' ' << QuoteArgument(job.SourcePath);

	std::string processOutput{};
	DWORD exitCode = 1;
	if (!RunProcess(ToWide(commandLine.str()), processOutput, exitCode))
	{
		out.Diagnostics.PrimaryErrorMessage = "Failed to launch DXC.";
		return false;
	}

	if (!processOutput.empty())
	{
		out.Diagnostics.InfoAndWarnings.push_back(processOutput);
	}

	if (exitCode != 0 || !std::filesystem::exists(outputPath))
	{
		out.Diagnostics.PrimaryErrorMessage = "DXC compilation failed.";
		return false;
	}

	if (!ReadFileBytes(outputPath, out.Bytecode))
	{
		out.Diagnostics.PrimaryErrorMessage = "Failed to read compiled shader bytecode.";
		return false;
	}

	std::error_code errorCode{};
	std::filesystem::remove(outputPath, errorCode);
	return true;
}
