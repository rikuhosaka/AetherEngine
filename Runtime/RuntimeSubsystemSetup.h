#pragma once

class DisplayServices;
class EngineLoopConfig;
class RenderServices;
class RHIServices;
class SubsystemContext;
class SubsystemRegistry;
class WindowSubsystem;
class RenderSubsystem;

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineLoopConfig& config);

[[nodiscard]] WindowSubsystem* FindWindowSubsystem(const SubsystemRegistry& registry);
[[nodiscard]] RenderSubsystem* FindRenderSubsystem(const SubsystemRegistry& registry);
[[nodiscard]] RHIServices* FindRHIServices(const SubsystemContext& context);
[[nodiscard]] DisplayServices* FindDisplayServices(const SubsystemContext& context);
