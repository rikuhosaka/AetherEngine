#pragma once

#include <string_view>

struct IUnknown;

namespace DX12GpuNaming
{
	void SetName(IUnknown* object, std::string_view name);

	void SetResourceName(
		IUnknown* resource,
		std::string_view prefix,
		const char* debugName = nullptr);

} // namespace DX12GpuNaming
