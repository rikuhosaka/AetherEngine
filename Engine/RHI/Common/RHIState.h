#pragma once


enum class RasterizerState
{
	Solid,
	Wireframe,
	NoCull,
	FrontCull
};

enum class BlendState
{
	Opaque,
	AlphaBlend,
	Additive,
	NonPremultiplied
};

enum class DepthStencilState
{
	DepthDefault,
	DepthReadOnly,
	DepthNone,
	StencilDefault
};