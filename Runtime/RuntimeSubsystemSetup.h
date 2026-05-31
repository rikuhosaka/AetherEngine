#pragma once

class EngineLoopConfig;
class RHIServices;
class SubsystemContext;
class SubsystemRegistry;
class WindowSubsystem;

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineLoopConfig& config);

[[nodiscard]] WindowSubsystem* FindWindowSubsystem(const SubsystemRegistry& registry);
[[nodiscard]] RHIServices* FindRHIServices(const SubsystemContext& context);
