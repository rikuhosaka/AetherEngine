#pragma once

#include <filesystem>

class Renderer;

struct RenderServices
{
	Renderer* renderer = nullptr;
	std::filesystem::path shaderRoot{};
};
