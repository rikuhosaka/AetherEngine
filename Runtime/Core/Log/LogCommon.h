#pragma once


enum class LogLevel
{
    Info,
    Warning,
    Error,
    Fatal
};

enum class LogCategory
{
    Core,
    Renderer,
    RHI,
    Asset,
    ECS,
    Physics,
    Animation
};