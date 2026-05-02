#include "Background.h"
#include "Math.h"
#include "ApplicationResource.h"
#include "Rendering/Renderer.h"
#include "Rendering/Framebuffer.h"
#include "ImGui/imgui_internal.h"

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
	Background::Background() : brightness{ 1.0f }, dirty{ true }, wantJacket{ true }
	{
		framebuffer = std::make_unique<Framebuffer>(1, 1);
	}

	void Background::process(Renderer* renderer, const Jacket& jacket)
	{
		if (framebuffer == nullptr)
			return;

		dirty = false;
		auto& resource = getResources().backgroundResources;
		const Texture* bgTexture = resource.getBackgroundTexture();
		const Texture* decoTexture = resource.getStageDecoTexture();
		const Sprite* whiteMask = resource.getMaskWhite();
		wantJacket = !bgTexture;
		if (!bgTexture)
			bgTexture = resource.getStageBGTexture();
		if (bgTexture == nullptr || decoTexture == nullptr || whiteMask == nullptr)
			return;

		float w = bgTexture->getWidth(), h = bgTexture->getHeight();
		if (w < 1 || h < 1)
			return;

		Shader* shader = getShader("basicMask");
		if (!shader)
			return;
		shader->use();
		shader->setInt("baseTex", 0);
		shader->setInt("maskTex", 1);
		// DirectXMath dies when projection size is too small
		w = std::max(w, 10.f), h = std::max(h, 10.f);
		shader->setMatrix4("projection",
		                   DirectX::XMMatrixOrthographicOffCenterRH(0, w, 0, h, 0.001f, 100.0f));

		framebuffer->resize(w, h);
		framebuffer->bind();
		framebuffer->clear(0.2, 0.2, 0.2, 1.f);

		ImDrawListSharedData* sharedData = ImGui::GetDrawListSharedData();
		if (!sharedData)
			return;

		renderer->beginBatch();

		renderer->setTexture(*bgTexture, *bgTexture->getDefaultSprite(), *decoTexture, *whiteMask);
		renderer->drawRectangle({ 0, 0 }, { w, h });

		int jacketTexID;

		if (!wantJacket || (jacketTexID = jacket.getTexID()) == 0)
		{
			renderer->endBatch();
			framebuffer->unbind();
			return;
		}

		Vector2 mainLeftPos{ 602, 816 }, mainLeftSize{ 264, 174 };
		Vector2 mainRightPos{ 1205, 629 }, mainRightSize{ 200, 114 };
		Vector2 mirrorLeftPos{ 615, 1170 }, mirrorLeftSize{ 256, 162 };
		Vector2 mirrorRightPos{ 1186, 1387 }, mirrorRightSize{ 196, 105 };
		// Draw the masked frame first
		renderer->setTexture(jacketTexID, decoTexture->getID(), resource.getJacketMaskLeftUV());
		renderer->drawRectangle(mainLeftPos, mainLeftSize);
		renderer->setTexture(jacketTexID, decoTexture->getID(), resource.getJacketMaskRightUV());
		renderer->drawRectangle(mainRightPos, mainRightSize);
		renderer->setTexture(jacketTexID, decoTexture->getID(),
		                     resource.getJacketMaskLeftMirrorUV());
		renderer->drawRectangle(mirrorLeftPos, mirrorLeftSize);
		renderer->setTexture(jacketTexID, decoTexture->getID(),
		                     resource.getJacketMaskRightMirrorUV());
		renderer->drawRectangle(mirrorRightPos, mirrorRightSize);
		// Draw the polka dot cover
		const Sprite* sprite = resource.getJacketLeftCover();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle(mainLeftPos, mainLeftSize);
		}
		sprite = resource.getJacketRightCover();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle(mainRightPos, mainRightSize);
		}
		sprite = resource.getJacketLeftCoverMirror();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle(mirrorLeftPos, mirrorLeftSize);
		}
		sprite = resource.getJacketRightCoverMirror();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle(mirrorRightPos, mirrorRightSize);
		}
		// Draw the main window
		sprite = resource.getJacketCenterWindow();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle({ 682, 497 }, { 686, 686 });
		}
		sprite = resource.getJacketCenterWindowMirror();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle({ 699, 958 }, { 651, 650 });
		}
		// Draw main masked jacket
		Vector2 mainCenterPos{ 824, 666 }, mainCenterSize{ 400, 384 };
		Vector2 mirrorCenterPos{ 834, 1120 }, mirrorCenterSize{ 386, 336 };
		renderer->setTexture(jacketTexID, decoTexture->getID(), resource.getJacketMaskCenterUV());
		renderer->drawRectangle(mainCenterPos, mainCenterSize);
		renderer->setTexture(jacketTexID, decoTexture->getID(),
		                     resource.getJacketMaskCenterMirrorUV());
		renderer->drawRectangle(mirrorCenterPos, mirrorCenterSize);
		// Draw main polka dot overlay
		sprite = resource.getJacketCenterCover();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle(mainCenterPos, mainCenterSize);
		}
		sprite = resource.getJacketCenterCoverMirror();
		if (sprite)
		{
			renderer->setTexture(*decoTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle(mirrorCenterPos, mirrorCenterSize);
		}
		// Draw the stage floor
		const Texture* stageTexture = resource.getStageTexture();
		sprite = resource.getStageFloor();
		if (stageTexture && sprite)
		{
			renderer->setTexture(*stageTexture, *sprite, *decoTexture, *whiteMask);
			renderer->drawRectangle({ 0, 1251 }, { sprite->getWidth(), sprite->getHeight() });
		}
		renderer->endBatch();
		framebuffer->unbind();

		dirty = false;
	}

	void Background::draw(ImDrawList* drawList, ImVec2 min, ImVec2 max) const
	{
		float scrWidth = max.x - min.x, scrHeight = max.y - min.y;
		float bgScrWidth = framebuffer->getWidth(), bgScrHeight = framebuffer->getHeight();
		fillRect(scrWidth, scrHeight, bgScrWidth / bgScrHeight, bgScrWidth, bgScrHeight);
		float ratioX = scrWidth / bgScrWidth, ratioY = scrHeight / bgScrHeight;
		ImVec2 uv1 = { (1 - ratioX) / 2, (1 - ratioY) / 2 };
		ImVec2 uv2 = { uv1.x + ratioX, uv1.y + ratioY };
		ImU32 col = ImGui::ColorConvertFloat4ToU32({ brightness, brightness, brightness, 1.0f });
		drawList->AddImage(framebuffer->getTexture(), min, max, uv1, uv2, col);
	}

	float Background::getBrightness() const { return brightness; }

	void Background::setBrightness(float b) { brightness = b; }

	bool Background::isDirty() const { return dirty; }

	void Background::setDirty() { dirty = true; }

	bool Background::hasJacket() const { return wantJacket; }

	void Background::dispose()
	{
		if (framebuffer)
		{
			framebuffer->dispose();
			framebuffer = nullptr;
		}
	}
}
