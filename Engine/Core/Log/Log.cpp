#include "Log.h"

#include <string_view>
#include <fstream>
#include <mutex>
#include <source_location>

namespace
{

const char* ToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Info:    return "Info";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Error:   return "Error";
    case LogLevel::Fatal:   return "Fatal";
    }

    return "Unknown";
}

const char* ToString(LogCategory category)
{
    switch (category)
    {
    case LogCategory::Core:      return "Core";
    case LogCategory::Renderer:  return "Renderer";
    case LogCategory::RHI:       return "RHI";
    case LogCategory::Asset:     return "Asset";
    case LogCategory::ECS:       return "ECS";
    case LogCategory::Physics:   return "Physics";
    case LogCategory::Animation: return "Animation";
    }

    return "Unknown";
}

}

Logger& Logger::Instance()
{
    static Logger logger;
    return logger;
}

bool Logger::Initialize(const std::string& filename)
{
    m_file.open(filename, std::ios::out | std::ios::trunc);

    return m_file.is_open();
}

void Logger::Shutdown()
{
    if (m_file.is_open())
    {
        m_file.close();
    }
}

void Logger::Write(
    LogCategory category,
    LogLevel level,
    std::string_view message,
    const std::source_location& location)
{
    std::scoped_lock lock(m_mutex);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::string output =
        std::format(
            "[{}][{}][{}:{}] {}\n",
            ToString(category),
            ToString(level),
            location.file_name(),
            location.line(),
            message);

    OutputDebugStringA(output.c_str());

    if (m_file.is_open())
    {
        m_file << output;
        m_file.flush();
    }

    if (level == LogLevel::Fatal)
    {
#ifdef _DEBUG
        __debugbreak();
#endif
        std::abort();
    }
}