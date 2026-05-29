#pragma once

#include "Log.h"
#include "Result.h"

#define LOG_INFO(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Info, \
        msg)

#define LOG_WARN(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Warning, \
        msg)

#define LOG_ERROR(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Error, \
        msg)

#define LOG_FATAL(category, msg) \
    Logger::Instance().Write( \
        category, \
        LogLevel::Fatal, \
        msg)

#define TRY_LOG(expr, category)     \
{                                   \
    auto result = (expr);           \
    if (!result)                    \
    {                               \
        LogResult(result, category);\
        return result;              \
    }                               \
}