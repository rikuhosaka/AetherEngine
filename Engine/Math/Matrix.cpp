#include "Engine/Math/Matrix.h"

#include <cstring>

namespace Aether::Math
{
using namespace DirectX;

void StoreMatrixForHlsl(float outMatrix4x4[16], FXMMATRIX matrix)
{
	XMFLOAT4X4 transposed{};
	XMStoreFloat4x4(&transposed, XMMatrixTranspose(matrix));
	std::memcpy(outMatrix4x4, transposed.m, sizeof(transposed.m));
}

XMMATRIX LoadMatrixFromHlsl(const float matrix4x4[16])
{
	XMFLOAT4X4 loaded{};
	std::memcpy(loaded.m, matrix4x4, sizeof(loaded.m));
	return XMMatrixTranspose(XMLoadFloat4x4(&loaded));
}

XMMATRIX Inverse(FXMMATRIX matrix)
{
	XMVECTOR determinant{};
	return XMMatrixInverse(&determinant, matrix);
}

XMMATRIX InverseTranspose(FXMMATRIX matrix)
{
	return XMMatrixTranspose(Inverse(matrix));
}
} // namespace Aether::Math
