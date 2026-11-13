#pragma once

#include "Runtime/Engine/Application/Subsystem/SubsystemTypes.h"
#include "Runtime/Core/Log/Result.h"

#include <cstdint>
#include <span>

class SubsystemContext;

// 責任: プロセスに1つあるエンジン機能の初期化、Tick、終了を定義する。
// 役割: ウィンドウ、入力、エンジン、編集カメラのように、特定の World に属さない処理の口。
// 所有: SubsystemRegistry が unique_ptr で持つ。再生中の物理やプレイカメラは World 配下の IWorldSubsystem が持つ。
class ISubsystem
{
public:
	virtual ~ISubsystem() = default;

	[[nodiscard]] virtual const char* GetName() const = 0;

	[[nodiscard]] virtual std::span<const char* const> GetDependencies() const
	{
		return {};
	}

	[[nodiscard]] virtual SubsystemTickGroup GetTickGroup() const
	{
		return SubsystemTickGroup::Game;
	}

	[[nodiscard]] virtual bool WantsTick() const
	{
		return false;
	}

	virtual Result<void> Initialize(SubsystemContext& ctx) = 0;
	virtual Result<void> PostInitialize(SubsystemContext& ctx);
	virtual void Tick(SubsystemContext& ctx, float deltaSeconds);
	virtual void FixedTick(SubsystemContext& ctx, float fixedDeltaSeconds);
	virtual Result<void> OnResize(SubsystemContext& ctx, uint32_t width, uint32_t height);
	virtual void Shutdown(SubsystemContext& ctx) = 0;
};
