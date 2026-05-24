#pragma once

enum class RHIShaderVisibility : uint8_t
{
	All,
	Vertex,
	Pixel,
	Compute,
	Geometry,
	Hull,
	Domain,
};

enum class RHIRootParameterKind : uint8_t
{
	Constants,
	DescriptorTable,
	RootCBV,
	RootSRV,
	RootUAV,
};

enum class RHIRootParamType : uint8_t
{
	CBV,
	SRV,
	UAV,
	StaticSampler,
};

struct RHIRootDescriptorRange
{
	RHIRootParamType type = RHIRootParamType::SRV;
	uint32_t count = 1;
	uint32_t baseRegister = 0;
	uint32_t space = 0;
	uint32_t offset = 0;
	bool unbounded = false;

	bool operator==(const RHIRootDescriptorRange&) const = default;
};

struct RHIRootConstants
{
	uint32_t num32BitValues = 0;
	uint32_t baseRegister = 0;
	uint32_t space = 0;

	bool operator==(const RHIRootConstants&) const = default;
};

struct RHIRootDescriptor
{
	RHIRootParamType type = RHIRootParamType::CBV;
	uint32_t shaderRegister = 0;
	uint32_t space = 0;

	bool operator==(const RHIRootDescriptor&) const = default;
};

enum class RHIFilterMode : uint8_t
{
	Point,
	Linear,
	Anisotropic,
};

enum class RHIAddressMode : uint8_t
{
	Wrap,
	Clamp,
	Mirror,
	Border,
};

struct RHIRootStaticSampler
{
	uint32_t shaderRegister = 0;
	uint32_t space = 0;
	RHIFilterMode filter = RHIFilterMode::Linear;
	RHIAddressMode addressU = RHIAddressMode::Wrap;
	RHIAddressMode addressV = RHIAddressMode::Wrap;
	RHIAddressMode addressW = RHIAddressMode::Wrap;
	RHIShaderVisibility visibility = RHIShaderVisibility::Pixel;

	bool operator==(const RHIRootStaticSampler&) const = default;
};

struct RHIRootParameterDesc
{
	RHIRootParameterKind kind = RHIRootParameterKind::Constants;
	RHIShaderVisibility visibility = RHIShaderVisibility::All;

	std::vector<RHIRootDescriptorRange> ranges;
	RHIRootConstants constants{};
	RHIRootDescriptor descriptor{};

	bool operator==(const RHIRootParameterDesc&) const = default;
};

enum class RHIRootSignatureFlags : uint32_t
{
	None = 0,
	AllowInputAssembler = 1 << 0,
	DenyVertexShaderAccess = 1 << 1,
	DenyPixelShaderAccess = 1 << 2,
	DenyComputeShaderAccess = 1 << 3,
	DirectlyIndexedHeap = 1 << 4,
};

inline RHIRootSignatureFlags operator|(RHIRootSignatureFlags a, RHIRootSignatureFlags b)
{
	return static_cast<RHIRootSignatureFlags>(
		static_cast<uint32_t>(a) |
		static_cast<uint32_t>(b));
}

struct RHIRootSignatureLayout
{
	std::vector<RHIRootParameterDesc> parameters;
	std::vector<RHIRootStaticSampler> staticSamplers;
	RHIRootSignatureFlags flags = RHIRootSignatureFlags::AllowInputAssembler;

	bool operator==(const RHIRootSignatureLayout&) const = default;
};

inline RHIRootSignatureLayout MakeDefaultForwardRootSignatureLayout()
{
	RHIRootSignatureLayout layout{};

	{
		RHIRootParameterDesc param{};
		param.kind = RHIRootParameterKind::Constants;
		param.visibility = RHIShaderVisibility::Vertex;
		param.constants.num32BitValues = 4;
		param.constants.baseRegister = 0;
		layout.parameters.push_back(param);
	}

	{
		RHIRootParameterDesc param{};
		param.kind = RHIRootParameterKind::DescriptorTable;
		param.visibility = RHIShaderVisibility::All;
		param.ranges.push_back({
			.type = RHIRootParamType::CBV,
			.count = 1,
			.baseRegister = 1,
		});
		layout.parameters.push_back(param);
	}

	{
		RHIRootParameterDesc param{};
		param.kind = RHIRootParameterKind::DescriptorTable;
		param.visibility = RHIShaderVisibility::Pixel;
		param.ranges.push_back({
			.type = RHIRootParamType::SRV,
			.count = 8,
			.baseRegister = 0,
		});
		layout.parameters.push_back(param);
	}

	layout.staticSamplers = {
		{ .shaderRegister = 0, .filter = RHIFilterMode::Linear },
		{
			.shaderRegister = 1,
			.filter = RHIFilterMode::Anisotropic,
			.addressU = RHIAddressMode::Clamp,
			.addressV = RHIAddressMode::Clamp,
		},
		{ .shaderRegister = 2, .filter = RHIFilterMode::Linear },
	};

	return layout;
}
