#pragma once

struct EngineConfig;
class SubsystemRegistry;
class WindowSubsystem;

void RegisterRuntimeSubsystems(SubsystemRegistry& registry, const EngineConfig& config);

[[nodiscard]] WindowSubsystem* FindWindowSubsystem(const SubsystemRegistry& registry);
