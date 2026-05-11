#pragma once

enum class LogLevel
{
    Info,
    Warning,
    Error,
    Fatal
};

void Log(LogLevel level, const std::string& message);

void Fatal(const std::string& message, const char* file, int line);