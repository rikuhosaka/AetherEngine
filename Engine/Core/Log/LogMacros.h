#pragma once

#include "Log.h"
#include "Result.h"

#include <format>
#include <source_location>

#define LOG_INFO(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Info, \
        msg, \
        std::source_location::current())

#define LOG_WARN(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Warning, \
        msg, \
        std::source_location::current())

#define LOG_ERROR(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Error, \
        msg, \
        std::source_location::current())

#define LOG_FATAL(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Fatal, \
        msg, \
        std::source_location::current())

// MSVC/clang-cl compatible std::format helper (##__VA_ARGS__ omits trailing comma when empty).
#define AETHER_LOG_FORMAT(fmt, ...) std::format(fmt, ##__VA_ARGS__)

// Formatted logging. Example:
//   LOG_INFO_F(LogCategory::Renderer, "fenceValue={}", fenceValue);
#define LOG_INFO_F(category, fmt, ...) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Info, \
        AETHER_LOG_FORMAT(fmt, ##__VA_ARGS__), \
        std::source_location::current())

#define LOG_WARN_F(category, fmt, ...) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Warning, \
        AETHER_LOG_FORMAT(fmt, ##__VA_ARGS__), \
        std::source_location::current())

#define LOG_ERROR_F(category, fmt, ...) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Error, \
        AETHER_LOG_FORMAT(fmt, ##__VA_ARGS__), \
        std::source_location::current())

#define LOG_FATAL_F(category, fmt, ...) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Fatal, \
        AETHER_LOG_FORMAT(fmt, ##__VA_ARGS__), \
        std::source_location::current())

#define TRY_LOG(expr, category)     \
{                                   \
    auto result = (expr);           \
    if (!result)                    \
    {                               \
        LogResult(result, category);\
        return result;              \
    }                               \
}

#define TRY_LOG_FATAL(expr, category) \
{                                     \
    auto result = (expr);             \
    if (!result)                      \
    {                                 \
        LogFatalResult(result, category);\
        return result;                \
    }                                 \
}