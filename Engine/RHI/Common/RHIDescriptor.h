#pragma once


struct CpuDescHandle { size_t ptr = 0; };
struct GpuDescHandle { uint64_t ptr = 0; };

struct CbvSrvUavHandle {
	CpuDescHandle cpu;
	GpuDescHandle gpu;
};

struct SamplerHandle {
	CpuDescHandle cpu;
	GpuDescHandle gpu;
};

struct RtvHandle {
	CpuDescHandle cpu;
};

struct DsvHandle {
	CpuDescHandle cpu;
};