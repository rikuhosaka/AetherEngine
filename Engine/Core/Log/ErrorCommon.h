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

    PipelineCreationFailed
};

struct ErrorInfo
{
    ErrorCode code = ErrorCode::None;
    std::string message;
};
