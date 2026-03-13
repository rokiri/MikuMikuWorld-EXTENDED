#include "Background.h"
#include "Math.h"
#include "ApplicationResource.h"
#include "Rendering/Renderer.h"
#include "Rendering/Framebuffer.h"

// Scale a rectangle with specified aspect ratio so it fill the area of the target rectangle
static void fillRect(float target_width, float target_height, long double source_aspect_ratio,
                     float& width, float& height)
{
	const float target_aspect_ratio = target_width / target_height;
	width = target_aspect_ratio < source_aspect_ratio ? source_aspect_ratio * target_height
	                                                  : target_width;
	height = target_aspect_ratio > source_aspect_ratio ? target_width / source_aspect_ratio
	                                                   : target_height;
	return;
}

namespace MikuMikuWorld
{
	Background::Background() : brightness{ 0.4f }, width{ 0 }, height{ 0 } //, dirty{ false }
	{
		// framebuffer = std::make_unique<Framebuffer>(1, 1);
	}

	void Background::load(const std::string& filename)
	{
		this->filename = filename;
		if (texture)
		{
			texture->dispose();
			texture.reset();
		}

		if (filename.empty() || !IO::File::exists(filename))
			return;

		texture = std::make_unique<Texture>(filename);
		// framebuffer->resize(texture->getWidth(), texture->getHeight());

		// dirty = true;
	}

	// void Background::resize(Vector2 target)
	//{
	//	if (framebuffer == nullptr || texture == nullptr)
	//		return;

	//	float w = texture->getWidth();
	//	float h = texture->getHeight();
	//	float tgtAspect = target.x / target.y;

	//	if (tgtAspect > 1.0f)
	//		resizeByRatio(w, h, target, false);
	//	else if (tgtAspect < 1.0f)
	//		resizeByRatio(w, h, target, true);
	//	else
	//		w = h = target.x;

	//	// for non-square aspect ratios
	//	if (h < target.y)
	//		resizeByRatio(w, h, target, true);
	//	else if (w < target.x)
	//		resizeByRatio(w, h, target, false);

	//	width = w;
	//	height = h;
	//}

	// void Background::process(Renderer* renderer)
	//{
	//	if (framebuffer == nullptr || texture == nullptr)
	//		return;

	//	int w = texture->getWidth();
	//	int h = texture->getHeight();

	//	if (w < 1 || h < 1)
	//		return;

	//	Shader* shader = getShader("basic2d");
	//	if (!shader)
	//		return;
	//	shader->use();
	//	shader->setMatrix4("projection", DirectX::XMMatrixOrthographicRH(w, -h, 0.001f, 100));

	//	framebuffer->bind();
	//	framebuffer->clear();
	//	renderer->beginBatch();

	//	// align background quad to canvas position
	//	Vector2 posR;
	//	Vector2 size(w, h);
	//	Color tint(brightness, brightness, brightness, 1.0f);

	//	renderer->drawSprite(posR, 0.0f, size, AnchorType::MiddleCenter, *texture,
	//	                     *texture->getDefaultSprite(), tint);
	//	renderer->endBatch();
	//	glDisable(GL_DEPTH_TEST);
	//	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//	dirty = false;
	//}

	void Background::draw(ImDrawList* drawList, ImVec2 min, ImVec2 max) const
	{
		float scrWidth = max.x - min.x, scrHeight = max.y - min.y;
		float bgScrWidth = texture->getWidth(), bgScrHeight = texture->getHeight();
		fillRect(scrWidth, scrHeight, bgScrWidth / bgScrHeight, bgScrWidth, bgScrHeight);
		float ratioX = scrWidth / bgScrWidth, ratioY = scrHeight / bgScrHeight;
		ImVec2 uv1 = { (1 - ratioX) / 2, (1 - ratioY) / 2 };
		ImVec2 uv2 = { uv1.x + ratioX, uv1.y + ratioY };
		ImU32 col = ImGui::ColorConvertFloat4ToU32({ brightness, brightness, brightness, 1.0f });
		drawList->AddImage(texture->getID(), min, max, uv1, uv2, col);
	}

	std::string Background::getFilename() const { return filename; }

	int Background::getWidth() const { return width; }

	int Background::getHeight() const { return height; }

	// int Background::getTextureID() const { return framebuffer->getTexture(); }

	float Background::getBrightness() const { return brightness; }

	void Background::setBrightness(float b)
	{
		brightness = b;
		// dirty = true;
	}

	// bool Background::isDirty() const { return dirty; }

	void Background::dispose()
	{
		// if (framebuffer)
		//{
		//	framebuffer->dispose();
		//	framebuffer = nullptr;
		// }

		if (texture)
		{
			texture->dispose();
			texture = nullptr;
		}
	}
}
