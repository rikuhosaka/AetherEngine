#include "Engine/RHI/DX12/Debug/DX12GpuNaming.h"

#if defined(AETHER_DX12_DEBUG) && AETHER_DX12_DEBUG

#include "Engine/RHI/DX12/pch.h"

#include <format>
#include <string>

namespace
{

std::wstring Utf8ToWide(std::string_view utf8)
{
	if (utf8.empty())
	{
		return {};
	}

	const int size = MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8.data(),
		static_cast<int>(utf8.size()),
		nullptr,
		0);
	if (size <= 0)
	{
		return {};
	}

	std::wstring wide(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(
		CP_UTF8,
		0,
		utf8.data(),
		static_cast<int>(utf8.size()),
		wide.data(),
		size);
	return wide;
}

void SetWideName(IUnknown* object, const std::wstring& wideName)
{
	if (object == nullptr || wideName.empty())
	{
		return;
	}

	ComPtr<ID3D12Object> d3d12Object;
	if (SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&d3d12Object))))
	{
		d3d12Object->SetName(wideName.c_str());
	}
}

} // namespace

namespace DX12GpuNaming
{

void SetName(IUnknown* object, std::string_view name)
{
	SetWideName(object, Utf8ToWide(name));
}

void SetResourceName(IUnknown* resource, std::string_view prefix, const char* debugName)
{
	if (resource == nullptr)
	{
		return;
	}

	if (debugName != nullptr && debugName[0] != '\0')
	{
		SetName(resource, std::format("{}/{}", prefix, debugName));
		return;
	}

	SetName(resource, prefix);
}

} // namespace DX12GpuNaming

#else

namespace DX12GpuNaming
{

void SetName(IUnknown*, std::string_view) {}

void SetResourceName(IUnknown*, std::string_view, const char*) {}

} // namespace DX12GpuNaming

#endif
