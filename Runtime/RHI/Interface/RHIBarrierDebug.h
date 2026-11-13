#pragma once

class RHIBarrierDebug
{
public:
	virtual ~RHIBarrierDebug() = default;

	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;
};
