#pragma once

#include "Log.h"
#include "Result.h"

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