#pragma once

#include <cstdint>

class IEngineLoopPlatform
{
public:
	virtual ~IEngineLoopPlatform() = default;

	virtual bool ProcessPlatformMessages() = 0;

	[[nodiscard]] virtual bool HasPendingResize() const = 0;
	virtual void ClearPendingResize() = 0;
	[[nodiscard]] virtual uint32_t GetPendingWidth() const = 0;
	[[nodiscard]] virtual uint32_t GetPendingHeight() const = 0;
};
