#pragma once
#include "Quad.h"
#include "../Math.h"
#include "Texture.h"
#include "AnchorType.h"
#include "VertexBuffer.h"
#include <vector>
#include <array>
#include <map>

namespace MikuMikuWorld
{
	class Renderer
	{
	  private:
		size_t numVertices{};
		size_t numBatchVertices{};
		size_t numIndices{};
		size_t numQuads{};
		size_t numBatchQuads{};

		struct DrawConfig
		{
			int channel;
			int texID;
			int texMaskID;
			int offset;
			int length;
		};
		std::vector<DrawConfig> draws;
		std::vector<Vertex> vertices;
		VertexBuffer vBuffer;
		std::array<DirectX::XMVECTOR, 4> vPos;
		std::array<DirectX::XMVECTOR, 4> uvCoords;
		std::array<DirectX::XMVECTOR, 4> vColor;

		bool batchStarted{ false };

		void resetRenderStats();
		void newDrawConfig(int channel, int texID = -1, int texMaskID = -1);
		void bindTexture(const DrawConfig& config);
		void unbindTexture();

	  public:
		Renderer();

		int getChannelID() const;
		void setChannelID(int id);

		void setTexture(const Texture& tex, const Sprite& spr);
		void setTexture(const Texture& tex, const Sprite& spr, const Texture& mask,
		                const Sprite& maskSpr);
		void setTexture(int texID, const Vector2& uvMin, const Vector2& uvMax);
		void setTexture(int texID, const std::array<DirectX::XMFLOAT4, 4>& uvs);
		void setTexture(int texID, int maskID, const Vector2& uvMin, const Vector2& uvMax,
		                const Vector2& muvMin, const Vector2& muvMax);
		void setTexture(int texID, int maskID, const std::array<DirectX::XMFLOAT4, 4>& uvs);

		void setColors(const Color& color);
		void setColors(const Color& c1, const Color& c2, const Color& c3, const Color& c4);
		void setColors(const std::array<DirectX::XMFLOAT4, 4>& colors);

		// Draw functions
		using Degree = float;
		void drawSprite(const Vector2& pos, Degree rot, const Vector2& size, AnchorType anchor);
		void drawRectangle(const Vector2& pos, const Vector2& size);

		void drawQuad(const Vector2& p1, const Vector2& p2, const Vector2& p3, const Vector2& p4);
		void drawQuad(const std::array<DirectX::XMFLOAT4, 4>& pos, const DirectX::XMMATRIX& m);

		void pushQuad();

		void beginBatch();
		void endBatch();

		inline int getNumVertices() const { return numBatchVertices; }
		inline int getNumQuads() const { return numBatchQuads; }
	};
}
