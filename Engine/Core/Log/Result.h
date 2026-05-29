#pragma once

#include "ErrorCommon.h"
#include "Log.h"
#include "LogCommon.h"

template<typename T>
struct [[nodiscard]] Result
{
    T value{};
    ErrorInfo error{};

    explicit operator bool() const
    {
        return error.code == ErrorCode::None;
    }
};

template<typename T>
void LogResult(
    const Result<T>& result,
    LogCategory category)
{
    if (result)
    {
        return;
    }

    Logger::Instance().Write(
        category,
        LogLevel::Error,
        result.error.message);
}