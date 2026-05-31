#include "Runtime/RuntimePaths.h"

#include <Windows.h>

std::filesystem::path ResolveCompiledShaderRoot()
{
	wchar_t modulePath[MAX_PATH]{};
	GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

	std::filesystem::path shaderRoot = std::filesystem::path(modulePath).parent_path();
	shaderRoot = shaderRoot / ".." / ".." / "CompiledShaders";
	std::error_code errorCode{};
	const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(shaderRoot, errorCode);
	return errorCode ? shaderRoot : canonicalPath;
}
