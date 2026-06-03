#include "Engine/RHI/DX12/Debug/DX12DebugSettings.h"

#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Log/LogCommon.h"
#include "Engine/Core/Log/LogMacros.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

DX12DebugSettingsData g_settings = DX12DebugSettingsData::Defaults();

std::string Trim(std::string value)
{
	const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
	while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
	{
		value.erase(value.begin());
	}
	while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
	{
		value.pop_back();
	}
	return value;
}

bool ParseBoolToken(std::string_view token, bool& outValue)
{
	std::string normalized(token);
	std::transform(
		normalized.begin(),
		normalized.end(),
		normalized.begin(),
		[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	if (normalized == "true" || normalized == "1")
	{
		outValue = true;
		return true;
	}
	if (normalized == "false" || normalized == "0")
	{
		outValue = false;
		return true;
	}
	return false;
}

bool ApplySetting(std::string_view key, std::string_view value)
{
	if (key == "enableDebugLayer")
	{
		return ParseBoolToken(value, g_settings.enableDebugLayer);
	}
	if (key == "gpuBasedValidation")
	{
		return ParseBoolToken(value, g_settings.gpuBasedValidation);
	}
	if (key == "enableDred")
	{
		return ParseBoolToken(value, g_settings.enableDred);
	}
	if (key == "enableGpuMarkers")
	{
		return ParseBoolToken(value, g_settings.enableGpuMarkers);
	}
	if (key == "usePixMarkers")
	{
		return ParseBoolToken(value, g_settings.usePixMarkers);
	}
	if (key == "loadPixGpuCapturer")
	{
		return ParseBoolToken(value, g_settings.loadPixGpuCapturer);
	}
	if (key == "enableFrameBarrierReport")
	{
		return ParseBoolToken(value, g_settings.enableFrameBarrierReport);
	}
	if (key == "logInfoQueueVerbose")
	{
		return ParseBoolToken(value, g_settings.logInfoQueueVerbose);
	}
	return false;
}

bool ParseJsonBool(const std::string& content, const char* key, bool& outValue)
{
	const std::string quotedKey = std::string("\"") + key + "\"";
	const size_t keyPos = content.find(quotedKey);
	if (keyPos == std::string::npos)
	{
		return false;
	}

	const size_t colonPos = content.find(':', keyPos + quotedKey.size());
	if (colonPos == std::string::npos)
	{
		return false;
	}

	size_t valueStart = colonPos + 1;
	while (valueStart < content.size() && std::isspace(static_cast<unsigned char>(content[valueStart])))
	{
		++valueStart;
	}

	size_t valueEnd = valueStart;
	while (valueEnd < content.size()
		&& content[valueEnd] != ','
		&& content[valueEnd] != '}'
		&& content[valueEnd] != '\n'
		&& content[valueEnd] != '\r')
	{
		++valueEnd;
	}

	return ParseBoolToken(std::string_view(content.data() + valueStart, valueEnd - valueStart), outValue);
}

void ParseLine(const std::string& line)
{
	const size_t delimiter = line.find('=');
	if (delimiter == std::string::npos)
	{
		return;
	}

	const std::string key = Trim(line.substr(0, delimiter));
	const std::string value = Trim(line.substr(delimiter + 1));
	ApplySetting(key, value);
}

} // namespace

DX12DebugSettingsData DX12DebugSettingsData::Defaults()
{
	DX12DebugSettingsData settings{};
#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG
	settings.enableDebugLayer = true;
	settings.enableDred = true;
	settings.enableGpuMarkers = true;
#if defined(AETHER_PIX) && AETHER_PIX
	settings.usePixMarkers = true;
#else
	settings.usePixMarkers = false;
#endif
#else
	settings.enableDebugLayer = false;
	settings.enableDred = false;
	settings.enableGpuMarkers = false;
	settings.usePixMarkers = false;
#endif
	return settings;
}

const DX12DebugSettingsData& DX12DebugSettingsData::Get()
{
	return g_settings;
}

void DX12DebugSettingsData::LoadFromFile(const std::filesystem::path& path)
{
	g_settings = Defaults();

	std::ifstream file(path);
	if (!file.is_open())
	{
		return;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	const std::string content = buffer.str();
	if (content.empty())
	{
		return;
	}

	bool parsedAny = false;
	if (ParseJsonBool(content, "enableDebugLayer", g_settings.enableDebugLayer))
	{
		parsedAny = true;
	}
	if (ParseJsonBool(content, "gpuBasedValidation", g_settings.gpuBasedValidation))
	{
		parsedAny = true;
	}
	if (ParseJsonBool(content, "enableDred", g_settings.enableDred))
	{
		parsedAny = true;
	}
	if (ParseJsonBool(content, "enableGpuMarkers", g_settings.enableGpuMarkers))
	{
		parsedAny = true;
	}
	if (ParseJsonBool(content, "usePixMarkers", g_settings.usePixMarkers))
	{
		parsedAny = true;
	}
	if (ParseJsonBool(content, "loadPixGpuCapturer", g_settings.loadPixGpuCapturer))
	{
		parsedAny = true;
	}
	if (ParseJsonBool(content, "enableFrameBarrierReport", g_settings.enableFrameBarrierReport))
	{
		parsedAny = true;
	}
	if (ParseJsonBool(content, "logInfoQueueVerbose", g_settings.logInfoQueueVerbose))
	{
		parsedAny = true;
	}

	if (!parsedAny)
	{
		std::istringstream lineStream(content);
		std::string line;
		while (std::getline(lineStream, line))
		{
			ParseLine(line);
		}
	}

	LOG_INFO(
		LogCategory::RHI,
		std::format(
			"Loaded DX12 debug settings from '{}' (debugLayer={} gpuValidation={} dred={} markers={} pixMarkers={} barrierReport={})",
			path.string(),
			g_settings.enableDebugLayer,
			g_settings.gpuBasedValidation,
			g_settings.enableDred,
			g_settings.enableGpuMarkers,
			g_settings.usePixMarkers,
			g_settings.enableFrameBarrierReport));
}
