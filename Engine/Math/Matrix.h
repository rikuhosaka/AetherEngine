#pragma once

#include <DirectXMath.h>

namespace Aether::Math
{
// Stores a matrix for HLSL `mul(matrix, float4)` (column-major constant buffer layout).
void StoreMatrixForHlsl(float outMatrix4x4[16], DirectX::FXMMATRIX matrix);

// Loads a matrix stored for HLSL column-major layout.
[[nodiscard]] DirectX::XMMATRIX LoadMatrixFromHlsl(const float matrix4x4[16]);

[[nodiscard]] DirectX::XMMATRIX Inverse(DirectX::FXMMATRIX matrix);

// Transposed inverse; use for transforming normals (worldInverseTranspose).
[[nodiscard]] DirectX::XMMATRIX InverseTranspose(DirectX::FXMMATRIX matrix);
} // namespace Aether::Math
