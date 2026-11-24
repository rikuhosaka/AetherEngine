#pragma once

#include "Runtime/Engine/World/EntityId.h"
#include "Runtime/Engine/World/RenderPose.h"
#include "Runtime/Engine/World/WorldCameraSample.h"

#include <span>
#include <vector>

class World;

// カメラのワールド姿勢とレンズを書く。CameraComponent が無ければ out を空にする。
void ExtractCameraView(
	const World& world,
	EntityId camera,
	WorldCameraSample& out,
	std::span<const RenderPose> poses = {});

// プレイ中の PhysicsSubsystem から、alpha の補間姿勢を集める。無ければ out を空にする。
// alpha 0 はステップ前、1 はステップ後。TransformComponent は変えない。
void SampleRenderPoses(const World& world, float alpha, std::vector<RenderPose>& out);

// 先頭のカメラから姿勢を書く。カメラが無ければ out を変えない。
void ExtractView(
	const World& world,
	WorldCameraSample& out,
	std::span<const RenderPose> poses = {});
