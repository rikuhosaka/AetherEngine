#pragma once

struct RHIServices;

// Drops command-list references and waits until the graphics queue is idle.
// Call this before releasing any GPU resource those lists have recorded.
void RetireGraphicsQueue(RHIServices* services);
