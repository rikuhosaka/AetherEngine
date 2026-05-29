#pragma once

#include "ErrorCommon.h"
#include "Log.h"
#include "LogCommon.h"

#include <string>
#include <utility>

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

template<>
struct [[nodiscard]] Result<void>
{
    ErrorInfo error{};

    explicit operator bool() const
    {
        return error.code == ErrorCode::None;
    }
};

template<typename T>
Result<T> MakeOk(T value)
{
    Result<T> result;
    result.value = std::move(value);
    return result;
}

inline Result<void> MakeOk()
{
    return Result<void>{};
}

template<typename T>
Result<T> MakeFail(ErrorCode code, std::string message)
{
    Result<T> result;
    result.error.code = code;
    result.error.message = std::move(message);
    return result;
}

inline Result<void> MakeFail(ErrorCode code, std::string message)
{
    Result<void> result;
    result.error.code = code;
    result.error.message = std::move(message);
    return result;
}

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
