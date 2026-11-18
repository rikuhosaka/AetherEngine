#pragma once

#include "Runtime/Core/Log/Result.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

// Whole-file binary read. Does not log; the caller chooses the category and message.
[[nodiscard]] inline Result<std::string> ReadBinaryFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return MakeFail<std::string>(
			ErrorCode::FileNotFound,
			"File could not be opened: " + path.string());
	}

	return MakeOk(std::string{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() });
}
