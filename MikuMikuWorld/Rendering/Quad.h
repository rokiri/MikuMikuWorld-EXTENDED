#pragma once
#include <DirectXMath.h>
#include <array>

namespace MikuMikuWorld
{
	struct Vertex
	{
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR color;
		DirectX::XMVECTOR uv;
	};
}