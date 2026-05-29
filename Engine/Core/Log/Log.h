#pragma once

#include "LogCommon.h"

#include <fstream>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>

class Logger
{
public:

    static Logger& Instance();

    bool Initialize(const std::string& filename = "Engine.log");

    void Shutdown();

    void Write(
        LogCategory category,
        LogLevel level,
        std::string_view message,
        const std::source_location& location =
            std::source_location::current());

private:

    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:

    std::ofstream m_file;
    std::mutex m_mutex;
};