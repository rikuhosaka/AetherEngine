#pragma once

class EngineLoopConfig;
class SubsystemRegistry;
class WindowSubsystem;

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineLoopConfig& config);

[[nodiscard]] WindowSubsystem* FindWindowSubsystem(const SubsystemRegistry& registry);
