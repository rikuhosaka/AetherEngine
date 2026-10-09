#pragma once

#include <filesystem>

class RenderResourceServices;

struct RenderServices
{
	RenderResourceServices* resources = nullptr;
	std::filesystem::path shaderRoot{};
};
