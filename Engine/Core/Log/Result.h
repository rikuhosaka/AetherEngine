#pragma once

#include "ErrorCommon.h"
#include "Log.h"
#include "LogCommon.h"

#include <source_location>
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
    LogCategory category,
    const std::source_location& location = std::source_location::current())
{
    if (result)
    {
        return;
    }

    Logger::Instance().Write(
        category,
        LogLevel::Error,
        result.error.message,
        location);
}

template<typename T>
void LogFatalResult(
    const Result<T>& result,
    LogCategory category,
    const std::source_location& location = std::source_location::current())
{
    if (result)
    {
        return;
    }

    Logger::Instance().Write(
        category,
        LogLevel::Fatal,
        result.error.message,
        location);
}

template<typename T>
Result<T> FailRuntime(
    LogCategory category,
    ErrorCode code,
    std::string message,
    const std::source_location& location = std::source_location::current())
{
    Logger::Instance().Write(category, LogLevel::Error, message, location);
    return MakeFail<T>(code, std::move(message));
}

template<typename T>
Result<T> FailInternal(
    LogCategory category,
    ErrorCode code,
    std::string message,
    const std::source_location& location = std::source_location::current())
{
    Logger::Instance().Write(category, LogLevel::Fatal, message, location);
    return MakeFail<T>(code, std::move(message));
}

inline Result<void> FailRuntime(
    LogCategory category,
    ErrorCode code,
    std::string message,
    const std::source_location& location = std::source_location::current())
{
    Logger::Instance().Write(category, LogLevel::Error, message, location);
    return MakeFail(code, std::move(message));
}

inline Result<void> FailInternal(
    LogCategory category,
    ErrorCode code,
    std::string message,
    const std::source_location& location = std::source_location::current())
{
    Logger::Instance().Write(category, LogLevel::Fatal, message, location);
    return MakeFail(code, std::move(message));
}
