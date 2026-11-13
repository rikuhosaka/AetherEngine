#pragma once

struct ExtractedLighting;
struct ExtractedView;
struct FrameConstants;
struct ObjectConstants;

void BuildFrameConstants(
	const ExtractedView& view,
	const ExtractedLighting& lighting,
	FrameConstants& outConstants);

void BuildObjectConstants(const float worldMatrix[16], ObjectConstants& outConstants);
