#include "Renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>

namespace DirectX
{
	static XMVECTOR XMLoadFloat4Ref(const XMFLOAT4& v) { return XMLoadFloat4(&v); }
}

namespace MikuMikuWorld
{
	constexpr size_t maxQuads = 100;

	Renderer::Renderer() : vBuffer{ maxQuads * 4 }
	{
		vBuffer.setup();
		draws.reserve(64);
		vertices.reserve(maxQuads * 4);
		draws.emplace_back();
		setColors(Color{ 1, 1, 1, 1 });
	}

	void Renderer::newDrawConfig(int channel, int texID, int texMaskID)
	{
		DrawConfig& config =
		    draws.back().length != 0 ? draws.emplace_back(draws.back()) : draws.back();
		config.offset += config.length;
		config.length = 0;
		config.channel = channel;
		if (texID >= 0)
			config.texID = texID;
		if (texMaskID >= 0)
			config.texMaskID = texMaskID;
	}

	void Renderer::bindTexture(const DrawConfig& config)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, config.texMaskID);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, config.texID);
	}

	void Renderer::unbindTexture()
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	int Renderer::getChannelID() const { return draws.back().channel; }

	void Renderer::setChannelID(int id)
	{
		if (draws.back().channel != id)
			newDrawConfig(id);
	}

	void Renderer::setTexture(const Texture& tex, const Sprite& spr)
	{
		auto&& [uv1, uv3] = tex.getCoords(spr);
		setTexture(tex.getID(), uv1, uv3);
	}

	void Renderer::setTexture(const Texture& tex, const Sprite& spr, const Texture& mask,
	                          const Sprite& maskSpr)
	{
		auto&& [uv1, uv3] = tex.getCoords(spr);
		auto&& [muv1, muv3] = mask.getCoords(maskSpr);
		setTexture(tex.getID(), mask.getID(), uv1, uv3, muv1, muv3);
	}

	void Renderer::setTexture(int texID, const Vector2& uv1, const Vector2& uv3)
	{
		Vector2 uv2 = { uv3.x, uv1.y }, uv4 = { uv1.x, uv3.y };
		std::array<DirectX::XMFLOAT4, 4> uvs = {};
		uvs[0] = { uv1.x, uv1.y, 0.f, 0.f };
		uvs[1] = { uv2.x, uv2.y, 0.f, 0.f };
		uvs[2] = { uv3.x, uv3.y, 0.f, 0.f };
		uvs[3] = { uv4.x, uv4.y, 0.f, 0.f };
		setTexture(texID, uvs);
	}

	void Renderer::setTexture(int texID, const std::array<DirectX::XMFLOAT4, 4>& uvs)
	{
		if (draws.back().texID != texID)
			newDrawConfig(getChannelID(), texID);
		std::transform(uvs.begin(), uvs.end(), uvCoords.begin(), DirectX::XMLoadFloat4Ref);
	}

	void Renderer::setTexture(int texID, int maskID, const Vector2& uv1, const Vector2& uv3,
	                          const Vector2& muv1, const Vector2& muv3)
	{
		Vector2 uv2 = { uv3.x, uv1.y }, uv4 = { uv1.x, uv3.y };
		Vector2 muv2 = { muv3.x, muv1.y }, muv4 = { muv1.x, muv3.y };
		std::array<DirectX::XMFLOAT4, 4> uvs = {};
		uvs[0] = { uv1.x, uv1.y, muv1.x, muv1.y };
		uvs[1] = { uv2.x, uv2.y, muv2.x, muv2.y };
		uvs[2] = { uv3.x, uv3.y, muv3.x, muv3.y };
		uvs[3] = { uv4.x, uv4.y, muv4.x, muv4.y };
		setTexture(texID, maskID, uvs);
	}

	void Renderer::setTexture(int texID, int maskID, const std::array<DirectX::XMFLOAT4, 4>& uvs)
	{
		if (draws.back().texID != texID || draws.back().texMaskID != maskID)
			newDrawConfig(getChannelID(), texID, maskID);
		std::transform(uvs.begin(), uvs.end(), uvCoords.begin(), DirectX::XMLoadFloat4Ref);
	}

	void Renderer::setColors(const Color& color) { setColors(color, color, color, color); }

	void Renderer::setColors(const Color& c1, const Color& c2, const Color& c3, const Color& c4)
	{
		std::array<DirectX::XMFLOAT4, 4> colors = {};
		colors[0] = { c1.r, c2.g, c3.b, c1.a };
		colors[1] = { c2.r, c2.g, c3.b, c2.a };
		colors[2] = { c3.r, c2.g, c3.b, c3.a };
		colors[3] = { c4.r, c2.g, c3.b, c4.a };
		setColors(colors);
	}

	void Renderer::setColors(const std::array<DirectX::XMFLOAT4, 4>& colors)
	{
		std::transform(colors.begin(), colors.end(), vColor.begin(), DirectX::XMLoadFloat4Ref);
	}

	static std::array<DirectX::XMFLOAT4, 4> getVPosAnchor(AnchorType type)
	{
		int row = (int)type / 3; // 0 = top, 1 = middle, 2 = bottom
		int col = (int)type % 3; // 0 = left, 1 = center, 2 = right

		// Compute offsets so that anchor maps to (0,0)
		float offsetX = (float)col * 0.5f; // 0, 0.5, 1
		float offsetY = (float)row * 0.5f; // 0, 0.5, 1

		// Shift to range [-1, 0] or [-0.5, 0.5] or [0, 1]
		float left = -offsetX;
		float right = left + 1.0f;

		float top = offsetY;
		float bottom = top - 1.0f;

		std::array<DirectX::XMFLOAT4, 4> vPos = {};
		vPos[0] = { left, top, 0.0f, 1.0f };
		vPos[1] = { right, top, 0.0f, 1.0f };
		vPos[2] = { right, bottom, 0.0f, 1.0f };
		vPos[3] = { left, bottom, 0.0f, 1.0f };
		return vPos;
	}

	static DirectX::XMMATRIX getModelMatrix(const Vector2& pos, const float rot, const Vector2& sz)
	{
		DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();
		model *= DirectX::XMMatrixScaling(sz.x, sz.y, 1.0f);
		model *= DirectX::XMMatrixRotationZ(DirectX::XMConvertToRadians(rot));
		model *= DirectX::XMMatrixTranslation(pos.x, pos.y, 0.0f);

		return model;
	}

	void Renderer::drawSprite(const Vector2& pos, Degree rot, const Vector2& size,
	                          AnchorType anchor)
	{
		DirectX::XMMATRIX model = getModelMatrix(pos, rot, size);
		std::array<DirectX::XMFLOAT4, 4> positions = getVPosAnchor(anchor);
		drawQuad(positions, model);
	}

	void Renderer::drawRectangle(const Vector2& pos, const Vector2& size)
	{
		Vector2 p1{ pos };
		Vector2 p2{ pos.x + size.x, pos.y };
		Vector2 p3{ pos + size };
		Vector2 p4{ pos.x, pos.y + size.y };
		drawQuad(p1, p2, p3, p4);
	}

	void Renderer::drawQuad(const Vector2& p1, const Vector2& p2, const Vector2& p3,
	                        const Vector2& p4)
	{
		std::array<DirectX::XMFLOAT4, 4> positions = {};
		positions[0] = { p1.x, p1.y, 0.0f, 1.0f };
		positions[1] = { p2.x, p2.y, 0.0f, 1.0f };
		positions[2] = { p3.x, p3.y, 0.0f, 1.0f };
		positions[3] = { p4.x, p4.y, 0.0f, 1.0f };
		std::transform(positions.begin(), positions.end(), vPos.begin(), DirectX::XMLoadFloat4Ref);
		pushQuad();
	}

	void Renderer::drawQuad(const std::array<DirectX::XMFLOAT4, 4>& pos, const DirectX::XMMATRIX& m)
	{
		std::transform(pos.begin(), pos.end(), vPos.begin(), [&](const DirectX::XMFLOAT4& v)
		               { return DirectX::XMVector2Transform(DirectX::XMLoadFloat4Ref(v), m); });
		pushQuad();
	}

	void Renderer::pushQuad()
	{
		for (int i = 0; i < 4; i++)
			vertices.emplace_back(Vertex{ vPos[i], vColor[i], uvCoords[i] });

		draws.back().length += 4;
		++numQuads;
		numVertices += 4;
		numIndices += 6;
	}

	void Renderer::resetRenderStats()
	{
		numIndices = 0;
		numVertices = 0;
		numQuads = 0;
	}

	void Renderer::beginBatch()
	{
		batchStarted = true;
		vBuffer.resetBufferPos();
		vertices.clear();
		resetRenderStats();
		if (draws.size() > 1)
			draws.erase(draws.begin(), std::prev(draws.end()));
		draws.back().offset = 0;
		draws.back().length = 0;
	}

	void Renderer::endBatch()
	{
		numBatchVertices = numVertices;
		numBatchQuads = numQuads;

		batchStarted = false;
		if (!vertices.size())
			return;

		std::stable_sort(draws.begin(), draws.end(), [](const DrawConfig& d1, const DrawConfig& d2)
		                 { return d1.channel < d2.channel; });

		vBuffer.bind();
		for (size_t i = 0; i < draws.size(); i++)
		{
			DrawConfig& config = draws[i];
			if (i != 0)
			{
				DrawConfig& lastConfig = draws[i - 1];
				if (config.texID != lastConfig.texID || config.texMaskID != lastConfig.texMaskID)
				{
					vBuffer.uploadBuffer();
					vBuffer.flushBuffer();
					vBuffer.resetBufferPos();
				}
			}
			bindTexture(config);
			size_t pushed = 0;
			do
			{
				pushed += vBuffer.pushBuffer(vertices.data() + config.offset + pushed,
				                             config.length - pushed);
				if (pushed < config.length)
				{
					vBuffer.uploadBuffer();
					vBuffer.flushBuffer();
					vBuffer.resetBufferPos();
				}
			} while (pushed < config.length);
		}
		vBuffer.uploadBuffer();
		vBuffer.flushBuffer();
		vBuffer.resetBufferPos();

		vertices.clear();
		unbindTexture();

	}
}