#pragma once


enum class ErrorCode
{
    None,

    InvalidArgument,

    FileNotFound,

    OutOfMemory,

    DeviceRemoved,

    DeviceLost,

    ShaderCompileFailed,

    PipelineCreationFailed,

    ResourceCreationFailed,

    RuntimeError,

    ShaderReflectionFailed
};

struct ErrorInfo
{
    ErrorCode code = ErrorCode::None;
    std::string message;
};
