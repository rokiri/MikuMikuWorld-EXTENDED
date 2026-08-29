#include "Application.h"
#include "ScorePreview.h"
#include "PreviewEngine.h"
#include "Rendering/Camera.h"
#include "ResourceManager.h"
#include "IO.h"
#include "Colors.h"
#include "ImageCrop.h"
#include "ApplicationConfiguration.h"
#include "Score.h"
#include "Utilities.h"
#include <glad/glad.h>

namespace MikuMikuWorld
{
	struct PreviewPlaybackState
	{
		bool isPlaying{}, wasLastFramePlaying{};
	} playbackState;

	constexpr float EFFECTS_TARGET_ASPECT = 16.f / 9.f;
	constexpr float MATH_PI = 3.14159265358979323846f;

	inline DirectX::XMFLOAT4 toFloat4(const Color& c, float alphaScale = 1.0f)
	{
		return { c.r, c.g, c.b, c.a * alphaScale };
	}

	namespace Utils
	{
		inline void fitRect(float target_width, float target_height,
		                    long double source_aspect_ratio, float& width, float& height)
		{
			const float target_aspect_ratio = target_width / target_height;
			width = target_aspect_ratio > source_aspect_ratio
			            ? (float)source_aspect_ratio * target_height
			            : target_width;
			height = target_aspect_ratio < source_aspect_ratio
			             ? target_width / (float)source_aspect_ratio
			             : target_height;
		}

		inline void fillRect(float target_width, float target_height,
		                     long double source_aspect_ratio, float& width, float& height)
		{
			const float target_aspect_ratio = target_width / target_height;
			width = target_aspect_ratio < source_aspect_ratio
			            ? (float)source_aspect_ratio * target_height
			            : target_width;
			height = target_aspect_ratio > source_aspect_ratio
			             ? target_width / (float)source_aspect_ratio
			             : target_height;
		}

		inline std::array<DirectX::XMFLOAT4, 4> getUV(float left, float right, float top,
		                                              float bottom)
		{
			return { { { right, top, 0.f, 1.f },
				       { right, bottom, 0.f, 1.f },
				       { left, bottom, 0.f, 1.f },
				       { left, top, 0.f, 1.f } } };
		}
	};

	static const int NOTE_SIDE_WIDTH = 91;
	static const int NOTE_SIDE_PAD = 10;
	static const int MAX_FLICK_SPRITES = 6;
	static const int HOLD_XCUTOFF = 36;
	static const int GUIDE_XCUTOFF = 3;
	static const int GUIDE_Y_TOP_CUTOFF = -41;
	static const int GUIDE_Y_BOTTOM_CUTOFF = -12;
	static Color defaultTint{ 1.f, 1.f, 1.f, 1.f };

	static double getCachedLayerScaledTime(const ScoreContext& context, int tick, int layer)
	{
		if (layer >= 0 && layer < static_cast<int>(context.scorePreviewDrawData.hsCache.size()) &&
		    !context.scorePreviewDrawData.hsCache[layer].nodes.empty())
		{
			return context.scorePreviewDrawData.hsCache[layer].getStm(tick);
		}

		return Engine::accumulateScaledDuration(tick, TICKS_PER_BEAT, context.score.tempoChanges,
		                                        context.score.hiSpeedChanges, layer);
	}

	static std::vector<double> getCurrentLayerScaledTimes(const ScoreContext& context)
	{
		std::vector<double> layerStm(context.score.layers.size());
		for (int i = 0; i < static_cast<int>(context.score.layers.size()); ++i)
			layerStm[i] = getCachedLayerScaledTime(context, context.currentTick, i);
		return layerStm;
	}

	ScorePreviewBackground::ScorePreviewBackground()
	    : backgroundFile(), jacketFile{}, brightness(0.5f), frameBuffer{ 2048, 2048 }, init{ false }
	{
	}

	ScorePreviewBackground::~ScorePreviewBackground() { frameBuffer.dispose(); }

	void ScorePreviewBackground::setBrightness(float value) { brightness = value; }

	void ScorePreviewBackground::update(Renderer* renderer, const Jacket& jacket)
	{
		init = true;
		backgroundFile = config.backgroundImage;
		jacketFile = jacket.getFilename();
		brightness = config.pvBackgroundBrightness;
		bool useDefaultTexture = backgroundFile.empty() || !IO::File::exists(backgroundFile);

		Texture backgroundTex = { useDefaultTexture
			                          ? Application::getAppDir() + "res\\textures\\default.png"
			                          : backgroundFile };
		const float bgWidth = backgroundTex.getWidth(), bgHeight = backgroundTex.getHeight();
		if (bgWidth != frameBuffer.getWidth() || bgHeight != frameBuffer.getHeight())
			frameBuffer.resize((unsigned int)bgWidth, (unsigned int)bgHeight);
		frameBuffer.bind();
		frameBuffer.clear();

		int shaderId;
		if ((shaderId = ResourceManager::getShader("basic2d")) == -1)
			return;
		Shader* basicShader = ResourceManager::shaders[shaderId];

		basicShader->use();

		const float projectionX{ std::max(bgWidth, 10.f) };
		const float projectionY{ std::max(bgHeight, 10.f) };
		basicShader->setMatrix4("projection", Camera().getOffCenterOrthographicProjection(
		                                          0, projectionX, 0, projectionY));

		if (backgroundTex.getID() > 0)
		{
			renderer->beginBatch();
			renderer->drawRectangle(Vector2(0, 0), Vector2(bgWidth, bgHeight), backgroundTex, 0,
			                        bgWidth, 0, bgHeight, defaultTint, 0);
			renderer->endBatch();
			if (useDefaultTexture && IO::File::exists(jacket.getFilename()))
				updateDrawDefaultJacket(renderer, jacket);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		backgroundTex.dispose();
	}

	void ScorePreviewBackground::updateDrawDefaultJacket(Renderer* renderer, const Jacket& jacket)
	{
		if (jacket.getFilename().empty())
			return;

		int index = ResourceManager::getTexture("stage");
		if (index == -1)
			return;

		const Texture& stage = ResourceManager::textures[index];
		if (stage.sprites.size() < STAGE_SPR_COUNT)
			return;

		int shaderId;
		if ((shaderId = ResourceManager::getShader("basic2d")) == -1)
			return;
		Shader* basicShader = ResourceManager::shaders[shaderId];

		if ((shaderId = ResourceManager::getShader("masking")) == -1)
			return;
		Shader* maskShader = ResourceManager::shaders[shaderId];

		const DirectX::XMFLOAT4 defCol{ 1.f, 1.f, 1.f, 1.f };
		const DirectX::XMFLOAT4 mainCol{ 1.f, 1.f, 1.f, 0.65f };
		const DirectX::XMFLOAT4 mirrorCol{ 1.f, 1.f, 1.f, 0.35f };

		auto mainLeftPos = Engine::quadvPos(602.f, 602.f + 264.f, 816.f, 816.f + 174.f);
		auto mainRightPos = Engine::quadvPos(1205.f, 1205.f + 200.f, 629.f, 629.f + 114.f);
		auto mirrorLeftPos = Engine::quadvPos(615.f, 615.f + 256.f, 1170.f, 1170.f + 162.f);
		auto mirrorRightPos = Engine::quadvPos(1186.f, 1186.f + 196.f, 1387.f, 1387.f + 105.f);

		auto mainLeftMask = Engine::quadUV(stage.sprites[SPR_JACKET_LEFT_MASK], stage);
		auto mainRightMask = Engine::quadUV(stage.sprites[SPR_JACKET_RIGHT_MASK], stage);
		auto mirrorLeftMask = Engine::quadUV(stage.sprites[SPR_MIRROR_JACKET_LEFT_MASK], stage);
		auto mirrorRightMask = Engine::quadUV(stage.sprites[SPR_MIRROR_JACKET_RIGHT_MASK], stage);

		maskShader->use();
		maskShader->setInt("baseTex", 0);
		maskShader->setInt("maskTex", 1);
		maskShader->setMatrix4(
		    "projection", Camera().getOffCenterOrthographicProjection(0.f, 2048.f, 0.f, 2048.f));
		renderer->pushQuadMasked(mainLeftPos, DefaultJacket::getLeftUV(), mainLeftMask, mainCol,
		                         jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mainRightPos, DefaultJacket::getRightUV(), mainRightMask, mainCol,
		                         jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mirrorLeftPos, DefaultJacket::getLeftMirrorUV(), mirrorLeftMask,
		                         mirrorCol, jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mirrorRightPos, DefaultJacket::getRightMirrorUV(), mirrorRightMask,
		                         mirrorCol, jacket.getTexID(), stage.getID());

		basicShader->use();
		basicShader->setMatrix4(
		    "projection", Camera().getOffCenterOrthographicProjection(0.f, 2048.f, 0.f, 2048.f));
		renderer->beginBatch();
		renderer->pushQuad(mainLeftPos, mainLeftMask, DirectX::XMMatrixIdentity(), defCol,
		                   stage.getID(), 0);
		renderer->pushQuad(mainRightPos, mainRightMask, DirectX::XMMatrixIdentity(), defCol,
		                   stage.getID(), 0);
		renderer->pushQuad(mirrorLeftPos, mirrorLeftMask, DirectX::XMMatrixIdentity(), defCol,
		                   stage.getID(), 0);
		renderer->pushQuad(mirrorRightPos, mirrorRightMask, DirectX::XMMatrixIdentity(), defCol,
		                   stage.getID(), 0);

		const Sprite* sprite = &stage.sprites[SPR_JACKET_WINDOW];
		renderer->drawRectangle(Vector2(682.f, 497.f), Vector2(686.f, 686.f), stage, sprite->getX(),
		                        sprite->getX() + sprite->getWidth(), sprite->getY(),
		                        sprite->getY() + sprite->getHeight(), defaultTint, 1);

		sprite = &stage.sprites[SPR_MIRROR_JACKET_WINDOW];
		renderer->drawRectangle(
		    Vector2(699.f, 958.f), Vector2(651.f, 650.f), stage, sprite->getX(),
		    sprite->getX() + sprite->getWidth(), sprite->getY(),
		    sprite->getY() + sprite->getHeight(),
		    Color(defaultTint.r, defaultTint.g, defaultTint.b, defaultTint.a * 0.6f), 1);
		renderer->endBatch();

		auto mainWindowPos = Engine::quadvPos(824.f, 824.f + 400.f, 666.f, 666.f + 384.f);
		auto mainWindowMask = Engine::quadUV(stage.sprites[SPR_JACKET_MASK], stage);
		auto mirrorWindowPos = Engine::quadvPos(834.f, 834.f + 386.f, 1120.f, 1120.f + 336.f);
		auto mirrorWindowMask = Engine::quadUV(stage.sprites[SPR_MIRROR_JACKET_MASK], stage);

		maskShader->use();
		renderer->pushQuadMasked(mainWindowPos, DefaultJacket::getCenterUV(), mainWindowMask,
		                         { 1.f, 1.f, 1.f, 0.8f }, jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mirrorWindowPos, DefaultJacket::getMirrorCenterUV(),
		                         mirrorWindowMask, { 1.f, 1.f, 1.f, 0.5f }, jacket.getTexID(),
		                         stage.getID());

		basicShader->use();
		renderer->beginBatch();
		renderer->pushQuad(mainWindowPos, mainWindowMask, DirectX::XMMatrixIdentity(), defCol,
		                   stage.getID(), 0);
		renderer->pushQuad(mirrorWindowPos, mirrorWindowMask, DirectX::XMMatrixIdentity(), defCol,
		                   stage.getID(), 0);

		sprite = &stage.sprites[SPR_SEKAI_FLOOR];
		renderer->drawRectangle(
		    Vector2(0.f, 1251.f), Vector2(sprite->getWidth(), sprite->getHeight()), stage,
		    sprite->getX(), sprite->getX() + sprite->getWidth(), sprite->getY(),
		    sprite->getY() + sprite->getHeight(),
		    Color(defaultTint.r, defaultTint.g, defaultTint.b, defaultTint.a * 0.8f), 1);
		renderer->endBatch();
	}

	bool ScorePreviewBackground::shouldUpdate(const Jacket& jacket) const
	{
		return !init || backgroundFile != config.backgroundImage ||
		       jacketFile != jacket.getFilename();
	}

	void ScorePreviewBackground::draw(Renderer* renderer, float scrWidth, float scrHeight) const
	{
		float bgScrWidth = (float)frameBuffer.getWidth(),
		      bgScrHeight = (float)frameBuffer.getHeight(), targetWidth, targetHeight;
		if (!backgroundFile.empty())
		{
			Utils::fillRect(scrWidth, scrHeight, bgScrWidth / bgScrHeight, bgScrWidth, bgScrHeight);
			targetWidth = Engine::STAGE_TARGET_WIDTH;
			targetHeight = Engine::STAGE_TARGET_HEIGHT;
		}
		else
		{
			bgScrWidth = Engine::BACKGROUND_SIZE;
			bgScrHeight = Engine::BACKGROUND_SIZE;
			targetWidth = Engine::STAGE_TARGET_WIDTH;
			targetHeight = Engine::STAGE_TARGET_HEIGHT;
		}

		const float bgWidth = bgScrWidth / (targetWidth * Engine::STAGE_WIDTH_RATIO);
		const float bgLeft = -bgWidth / 2.f;
		const float bgHeight = bgScrHeight / (targetHeight * Engine::STAGE_HEIGHT_RATIO);
		const float centerY =
		    0.5f / Engine::STAGE_HEIGHT_RATIO + Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const float bgTop = centerY + -bgHeight / 2.f;
		auto vPos = Engine::quadvPos(bgLeft, bgLeft + bgWidth, bgTop, bgTop + bgHeight);
		auto uv = Utils::getUV(0.f, 1.f, 0.f, 1.f);
		renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(),
		                   DirectX::XMFLOAT4(brightness, brightness, brightness, 1.f),
		                   frameBuffer.getTexture(), -10);
	}

	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getLeftUV()
	{
		return { { { 303.8f / 740, 504.8f / 740, 0, 0 },
			       { 317.5f / 740, 297.7f / 740, 0, 0 },
			       { 5.5f / 740, 278.3f / 740, 0, 0 },
			       { -8.f / 740, 497.4f / 740, 0, 0 } } };
	}
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getRightUV()
	{
		return { { { 749.5f / 740, 377.7f / 740, 0, 0 },
			       { 738.2f / 740, 188.1f / 740, 0, 0 },
			       { 415.0f / 740, 171.4f / 740, 0, 0 },
			       { 432.1f / 740, 363.9f / 740, 0, 0 } } };
	}
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getLeftMirrorUV()
	{
		return { { { 292.761414f / 740, 247.401382f / 740, 0, 0 },
			       { 310.765869f / 740, 491.944763f / 740, 0, 0 },
			       { 6.892246f / 740, 498.470642f / 740, 0, 0 },
			       { -6.246704f / 740, 258.264862f / 740, 0, 0 } } };
	}
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getRightMirrorUV()
	{
		return { { { 733.444458f / 740, 183.954681f / 740, 0, 0 },
			       { 743.541321f / 740, 355.960449f / 740, 0, 0 },
			       { 418.899414f / 740, 332.759491f / 740, 0, 0 },
			       { 410.746246f / 740, 155.907684f / 740, 0, 0 } } };
	}
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getCenterUV()
	{
		return { { { 755.541687f / 740, 744.057861f / 740, 0, 0 },
			       { 739.961182f / 740, -1.859504f / 740, 0, 0 },
			       { 0.043696f / 740, -1.859504f / 740, 0, 0 },
			       { -17.484388f / 740, 744.057861f / 740, 0, 0 } } };
	}
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getMirrorCenterUV()
	{
		return { { { 747.697083f / 740, 2.164453f / 740, 0, 0 },
			       { 743.909424f / 740, 731.297241f / 740, 0, 0 },
			       { -1.864066f / 740, 731.297241f / 740, 0, 0 },
			       { 3.837242f / 740, 2.164453f / 740, 0, 0 } } };
	}

	ScorePreviewWindow::ScorePreviewWindow()
	    : previewBuffer{ 1920, 1080 }, background(), scaledAspectRatio(1)
	{
		noteEffectsCamera.setFov(50.f);
		noteEffectsCamera.setRotation(-90.f, 27.1f);
		noteEffectsCamera.setPosition({ 0, 5.32f, -5.86f, 0 });
		noteEffectsCamera.positionCamNormal();

		const std::string comboDir = Application::getAppDir() + "res\\textures\\combo\\";
		for (int i = 0; i <= 9; i++)
			ResourceManager::loadTexture(comboDir + "p" + std::to_string(i) + ".png");
		ResourceManager::loadTexture(comboDir + "pt.png");

		const std::string scoreDir = Application::getAppDir() + "res\\textures\\score\\";
		const std::string lifeDir = Application::getAppDir() + "res\\textures\\life\\";

		ResourceManager::loadTexture(scoreDir + "bg.png");
		ResourceManager::loadTexture(scoreDir + "bar.png");
		ResourceManager::loadTexture(scoreDir + "fg.png");
		ResourceManager::loadTexture(scoreDir + "judge\\perfect.png");
		for (char r : { 'd', 'c', 'b', 'a', 's' })
		{
			ResourceManager::loadTexture(scoreDir + "rank\\chr\\" + r + ".png");
			ResourceManager::loadTexture(scoreDir + "rank\\txt\\en\\" + r + ".png");
		}
		for (int i = 0; i <= 9; i++)
		{
			ResourceManager::loadTexture(scoreDir + "digit\\" + std::to_string(i) + ".png");
			ResourceManager::loadTexture(scoreDir + "digit\\s" + std::to_string(i) + ".png");
			ResourceManager::loadTexture(lifeDir + "digit\\" + std::to_string(i) + ".png");
			ResourceManager::loadTexture(lifeDir + "digit\\s" + std::to_string(i) + ".png");
			ResourceManager::loadTexture(scoreDir + "digit\\n.png");
			ResourceManager::loadTexture(scoreDir + "digit\\sn.png");
		}
		ResourceManager::loadTexture(lifeDir + "bg.png");
		ResourceManager::loadTexture(lifeDir + "normal.png");
		const std::string skillEffectDir =
		    Application::getAppDir() + "res\\textures\\skilleffect\\";
		ResourceManager::loadTexture(skillEffectDir + "lifeup.png");
		ResourceManager::loadTexture(skillEffectDir + "scoreup.png");
		ResourceManager::loadTexture(lifeDir + "digit\\percentage.png");
		ResourceManager::loadTexture(lifeDir + "digit\\n.png");
	}

	ScorePreviewWindow::~ScorePreviewWindow() {}

	static int getComboAtTick(const Score& score, int currentTick)
	{
		int combo = 0;
		constexpr int halfBeat = TICKS_PER_BEAT / 2;

		for (const auto& [id, note] : score.notes)
		{
			if (note.tick > currentTick)
				continue;
			if (note.dummy)
				continue;
			if (note.getType() == NoteType::HoldMid)
				continue;

			const NoteType type = note.getType();
			const HoldNote* hold = nullptr;
			if (type == NoteType::Hold)
			{
				auto holdIt = score.holdNotes.find(id);
				if (holdIt != score.holdNotes.end())
					hold = &holdIt->second;
			}
			else if (type == NoteType::HoldEnd)
			{
				auto holdIt = score.holdNotes.find(note.parentID);
				if (holdIt != score.holdNotes.end())
					hold = &holdIt->second;
			}

			if (hold != nullptr)
			{
				if (hold->isGuide())
					continue;
				if (type == NoteType::Hold && hold->startType != HoldNoteType::Normal)
					continue;
				if (type == NoteType::HoldEnd && hold->endType != HoldNoteType::Normal)
					continue;
			}
			combo++;
		}

		for (const auto& [id, hold] : score.holdNotes)
		{
			if (hold.isGuide())
				continue;
			if (hold.dummy)
				continue;
			auto startIt = score.notes.find(id);
			auto endIt = score.notes.find(hold.end);
			if (startIt == score.notes.end() || endIt == score.notes.end())
				continue;
			int startTick = startIt->second.tick;
			int endTick = endIt->second.tick;
			if (startTick > currentTick)
				continue;
			int eighthTick = startTick + halfBeat;
			if (eighthTick % halfBeat)
				eighthTick -= (eighthTick % halfBeat);
			if (eighthTick == startTick || eighthTick >= endTick)
				continue;
			int clampedEnd = std::min(endTick, currentTick);
			if (clampedEnd % halfBeat)
				clampedEnd += halfBeat - (clampedEnd % halfBeat);
			clampedEnd = std::min(clampedEnd, endTick);
			if (clampedEnd > eighthTick)
				combo += (clampedEnd - eighthTick) / halfBeat;
			auto endNoteIt = score.notes.find(hold.end);
			if (endNoteIt != score.notes.end() && endNoteIt->second.tick <= currentTick &&
			    hold.endType == HoldNoteType::Normal && !endNoteIt->second.dummy)
			{
				combo++;
			}
		}

		return combo;
	}

	static int s_lastCombo = -1;
	static float s_comboAnimTimer = 0.f;

	static void drawComboOverlay(const ScoreContext& context, ImDrawList* drawList, ImVec2 position,
	                             ImVec2 size)
	{
		if (!drawList || size.x <= 1.f || size.y <= 1.f)
			return;

		const int combo = getComboAtTick(context.score, context.currentTick);
		if (combo <= 0)
			return;

		ImGuiIO& io = ImGui::GetIO();
		if (combo != s_lastCombo)
		{
			s_comboAnimTimer = 0.f;
			s_lastCombo = combo;
		}
		s_comboAnimTimer = std::min(s_comboAnimTimer + io.DeltaTime / 0.15f, 1.f);
		auto easeOut = [](float t) { return 1.f - (1.f - t) * (1.f - t) * (1.f - t); };
		const float animScale = 0.75f + 0.25f * easeOut(s_comboAnimTimer);

		const float uiScale = std::min(size.x / 1920.f, size.y / 1080.f);
		const float offsetX = (size.x - 1920.f * uiScale) * 0.5f;
		const float offsetY = (size.y - 1080.f * uiScale) * 0.5f;

		auto px = [&](float x) { return position.x + offsetX + x * uiScale; };
		auto py = [&](float y) { return position.y + offsetY + y * uiScale; };
		auto ps = [&](float v) { return v * uiScale; };

		constexpr float COMBO_DIGIT_STEP = 52.f;
		constexpr float COMBO_GROUP_CENTER_X = 1634.f;
		constexpr float COMBO_GROUP_CENTER_Y = 478.f;
		constexpr float COMBO_GROUP_SCALE = 1.8f;
		constexpr float COMBO_GROUP_OFFSET_Y = -12.f;
		constexpr float COMBO_BASE_SCALE = 1.0f;
		constexpr float comboCenterYOffset = 18.f;

		auto comboGroupX = [&](float x)
		{ return COMBO_GROUP_CENTER_X + (x - COMBO_GROUP_CENTER_X) * COMBO_GROUP_SCALE; };
		auto comboGroupY = [&](float y)
		{
			return COMBO_GROUP_CENTER_Y + (y - COMBO_GROUP_CENTER_Y) * COMBO_GROUP_SCALE +
			       COMBO_GROUP_OFFSET_Y;
		};
		auto comboGroupS = [&](float v) { return v * COMBO_GROUP_SCALE; };

		const std::string comboDir = Application::getAppDir() + "res\\textures\\combo\\";

		const int labelIdx = ResourceManager::getTextureByFilename(comboDir + "pt.png");
		if (labelIdx != -1)
		{
			const Texture& labelTex = ResourceManager::textures[labelIdx];
			constexpr float comboTagWidth = 180.f;
			constexpr float comboTagHeight = 60.f;
			const float x = px(comboGroupX(COMBO_GROUP_CENTER_X - comboTagWidth * 0.5f));
			const float y = py(comboGroupY(COMBO_GROUP_CENTER_Y - 52.f - comboTagHeight * 0.5f));
			const float w = ps(comboGroupS(comboTagWidth));
			const float h = ps(comboGroupS(comboTagHeight));
			drawList->AddImage((ImTextureID)(size_t)labelTex.getID(), ImVec2(x, y),
			                   ImVec2(x + w, y + h), ImVec2(0, 0), ImVec2(1, 1),
			                   IM_COL32(255, 255, 255, 255));
		}

		const std::string comboText = std::to_string(combo);
		const float mid = static_cast<float>(comboText.size()) / 2.f;

		for (size_t i = 0; i < comboText.size(); ++i)
		{
			const char ch = comboText[i];
			const int digitIdx =
			    ResourceManager::getTextureByFilename(comboDir + "p" + ch + ".png");
			if (digitIdx == -1)
				continue;
			const Texture& digitTex = ResourceManager::textures[digitIdx];

			const float left =
			    (static_cast<float>(i) - mid + 0.5f) * COMBO_DIGIT_STEP * COMBO_BASE_SCALE;
			const float centerX = comboGroupX(COMBO_GROUP_CENTER_X + left);
			const float centerY =
			    comboGroupY(COMBO_GROUP_CENTER_Y + comboCenterYOffset * COMBO_BASE_SCALE);
			const float h = ps(comboGroupS(134.f * COMBO_BASE_SCALE * animScale));
			const float w = h * (static_cast<float>(digitTex.getWidth()) /
			                     static_cast<float>(digitTex.getHeight()));

			drawList->AddImage((ImTextureID)(size_t)digitTex.getID(),
			                   ImVec2(px(centerX) - w * 0.5f, py(centerY) - h * 0.5f),
			                   ImVec2(px(centerX) + w * 0.5f, py(centerY) + h * 0.5f), ImVec2(0, 0),
			                   ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
		}
	}

	static void drawHudImage(ImDrawList* drawList, const Texture& tex, float x, float y, float w,
	                         float h, float alpha = 1.f)
	{
		if (!drawList || tex.getID() == 0 || w <= 0.1f || h <= 0.1f)
			return;
		const int a = static_cast<int>(std::max(0.f, std::min(1.f, alpha)) * 255.f);
		if (a <= 0)
			return;
		drawList->AddImage((ImTextureID)(size_t)tex.getID(), ImVec2(x, y), ImVec2(x + w, y + h),
		                   ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, a));
	}

	static void drawHudImageClipX(ImDrawList* drawList, const Texture& tex, float x, float y,
	                              float w, float h, float ratio, float alpha = 1.f)
	{
		if (!drawList || tex.getID() == 0 || w <= 0.1f || h <= 0.1f)
			return;
		const float r = std::max(0.f, std::min(1.f, ratio));
		if (r <= 0.001f)
			return;
		const int a = static_cast<int>(std::max(0.f, std::min(1.f, alpha)) * 255.f);
		if (a <= 0)
			return;
		drawList->AddImage((ImTextureID)(size_t)tex.getID(), ImVec2(x, y), ImVec2(x + w * r, y + h),
		                   ImVec2(0, 0), ImVec2(r, 1), IM_COL32(255, 255, 255, a));
	}

	static const Texture* getHudTexture(const std::string& subfolder, const std::string& filename)
	{
		const std::string path =
		    Application::getAppDir() + "res\\textures\\" + subfolder + "\\" + filename;
		const int idx = ResourceManager::getTextureByFilename(path);
		return idx >= 0 ? &ResourceManager::textures[idx] : nullptr;
	}

	static std::string scoreDigitsText(int score)
	{
		std::string text = std::to_string(std::max(0, score));
		while (text.size() < 8)
			text.insert(text.begin(), 'n');
		return text;
	}

	static void drawHudOverlay(const ScoreContext& context, ImDrawList* drawList, ImVec2 position,
	                           ImVec2 size)
	{
		if (!drawList || size.x <= 1.f || size.y <= 1.f)
			return;

		const float uiScale = std::min(size.x / 1920.f, size.y / 1080.f);
		const float offsetX = (size.x - 1920.f * uiScale) * 0.5f;

		auto px = [&](float x) { return position.x + offsetX + x * uiScale; };
		auto tpy = [&](float y) { return position.y + y * uiScale; };
		auto ps = [&](float v) { return v * uiScale; };

		auto drawTop =
		    [&](const Texture* tex, float x, float y, float w, float h, float alpha = 1.f)
		{
			if (tex)
				drawHudImage(drawList, *tex, px(x), tpy(y), ps(w), ps(h), alpha);
		};
		auto drawTopClipX = [&](const Texture* tex, float x, float y, float w, float h, float ratio,
		                        float alpha = 1.f)
		{
			if (tex)
				drawHudImageClipX(drawList, *tex, px(x), tpy(y), ps(w), ps(h), ratio, alpha);
		};

		constexpr float SCORE_ROOT_SCALE = 1.5f;
		constexpr float SCORE_BAR_W = 354.f;
		constexpr float HUD_LIFE_Y = 11.f;
		constexpr float HUD_LIFE_H = 104.f;

		auto sx = [&](float v) { return 36.f + v * SCORE_ROOT_SCALE; };
		auto sy = [&](float v) { return -3.f + v * SCORE_ROOT_SCALE; };
		auto ss = [&](float v) { return v * SCORE_ROOT_SCALE; };

		// Resolve current score/rank from drawData
		const auto& drawData = context.scorePreviewDrawData;
		const auto& comboTimes = drawData.comboTimes;
		const float chartTime = static_cast<float>(context.getTimeAtCurrentTick());

		int latestComboIndex = -1;
		if (!comboTimes.empty())
		{
			const auto it =
			    std::upper_bound(comboTimes.begin(), comboTimes.end(), chartTime + 0.0001f);
			latestComboIndex = static_cast<int>(std::distance(comboTimes.begin(), it)) - 1;
		}

		const int hudScore =
		    (latestComboIndex >= 0 && latestComboIndex < (int)drawData.hudScores.size())
		        ? drawData.hudScores[latestComboIndex]
		        : 0;
		const char hudRank =
		    (latestComboIndex >= 0 && latestComboIndex < (int)drawData.hudRanks.size())
		        ? drawData.hudRanks[latestComboIndex]
		        : 'd';
		const float hudScoreBar =
		    (latestComboIndex >= 0 && latestComboIndex < (int)drawData.hudScoreBars.size())
		        ? drawData.hudScoreBars[latestComboIndex]
		        : 0.f;

		if (config.pvDrawScoreHud)
		{
			drawTop(getHudTexture("score", "bg.png"), sx(0), sy(0), ss(444), ss(96));
			drawTopClipX(getHudTexture("score", "bar.png"), sx(79), sy(37), ss(SCORE_BAR_W), ss(16),
			             hudScoreBar);
			drawTop(getHudTexture("score", "fg.png"), sx(0), sy(0), ss(444), ss(96));
			drawTop(getHudTexture("score", std::string("rank\\chr\\") + hudRank + ".png"), sx(10),
			        sy(13), ss(49), ss(58));
			drawTop(getHudTexture("score", std::string("rank\\txt\\en\\") + hudRank + ".png"),
			        sx(6), sy(77), ss(60), ss(8));

			const std::string scoreText = scoreDigitsText(hudScore);
			for (size_t i = 0; i < scoreText.size(); ++i)
			{
				const char ch = scoreText[i];
				const Texture* shadow =
				    getHudTexture("score", std::string("digit\\s") + ch + ".png");
				const Texture* main = getHudTexture("score", std::string("digit\\") + ch + ".png");
				if (!shadow || !main)
					continue;

				const float slotX = sx(82.f + static_cast<float>(i) * 22.f);
				const float slotY = sy(60.f);
				const float shadowH = ps(ss(36.f));
				const float mainH = ps(ss(29.f));
				const float shadowW =
				    shadowH * ((float)shadow->getWidth() / (float)shadow->getHeight());
				const float mainW = mainH * ((float)main->getWidth() / (float)main->getHeight());
				const float centerX = px(slotX + ss(11.f));
				drawHudImage(drawList, *shadow, centerX - shadowW * 0.5f, tpy(slotY - ss(4.f)),
				             shadowW, shadowH);
				drawHudImage(drawList, *main, centerX - mainW * 0.5f, tpy(slotY), mainW, mainH);
			}
		}

		if (config.pvDrawLifeHud)
		{
			const bool isExtended = context.score.metadata.isExtendedScore;
			const float LIFE_DEFAULT =
			    isExtended ? static_cast<float>(context.score.metadata.baseLifePoint) : 1000.f;
			const float LIFE_FULL = LIFE_DEFAULT;
			const float LIFE_MAX =
			    isExtended ? static_cast<float>(context.score.metadata.baseLifePoint + 1000)
			               : 2000.f;

			const auto& lifeTimes = drawData.lifeTimes;
			int latestLifeIndex = -1;
			if (!lifeTimes.empty())
			{
				const auto it =
				    std::upper_bound(lifeTimes.begin(), lifeTimes.end(), chartTime + 0.0001f);
				latestLifeIndex = static_cast<int>(std::distance(lifeTimes.begin(), it)) - 1;
			}

			const int currentLife =
			    (latestLifeIndex >= 0 && latestLifeIndex < (int)drawData.hudLives.size())
			        ? drawData.hudLives[latestLifeIndex]
			        : static_cast<int>(LIFE_DEFAULT);

			const float lifeRatio =
			    std::max(0.f, std::min(1.f, static_cast<float>(currentLife) / LIFE_FULL));

			drawTop(getHudTexture("life", "bg.png"), 1442.f, HUD_LIFE_Y, 444.f, HUD_LIFE_H);
			drawTopClipX(getHudTexture("life", "normal.png"), 1442.f, HUD_LIFE_Y, 444.f, HUD_LIFE_H,
			             lifeRatio);

			const std::string lifeText = std::to_string(currentLife);
			for (size_t i = 0; i < lifeText.size(); ++i)
			{
				const char ch = lifeText[lifeText.size() - 1 - i];
				const Texture* shadow =
				    getHudTexture("life", std::string("digit\\s") + ch + ".png");
				const Texture* main = getHudTexture("life", std::string("digit\\") + ch + ".png");
				if (!shadow || !main)
					continue;

				const float slotX = 1442.f + 319.f - static_cast<float>(i) * 22.f;
				const float shadowH = ps(37.f);
				const float mainH = ps(34.f);
				const float shadowW =
				    shadowH * ((float)shadow->getWidth() / (float)shadow->getHeight());
				const float mainW = mainH * ((float)main->getWidth() / (float)main->getHeight());
				const float centerX = px(slotX + 13.f);
				drawHudImage(drawList, *shadow, centerX - shadowW * 0.5f, tpy(19.f), shadowW,
				             shadowH);
				drawHudImage(drawList, *main, centerX - mainW * 0.5f, tpy(21.f), mainW, mainH);
			}
		}
	}

	static void drawSkillLifePopup(const ScoreContext& context, ImDrawList* drawList,
	                               ImVec2 position, ImVec2 size)
	{
		if (!drawList || size.x <= 1.f || size.y <= 1.f)
			return;

		const auto& drawData = context.scorePreviewDrawData;
		const auto& lifeTimes = drawData.lifeTimes;
		if (lifeTimes.empty())
			return;

		constexpr float POPUP_SLIDE_IN = 0.18f;
		constexpr float POPUP_SLIDE_OUT = 0.22f;
		constexpr float POPUP_TOTAL = 2.0f;
		const float LIFE_DEFAULT = context.score.metadata.isExtendedScore
		                               ? static_cast<float>(context.score.metadata.baseLifePoint)
		                               : 1000.f;

		const float chartTime = static_cast<float>(context.getTimeAtCurrentTick());

		const auto it = std::upper_bound(lifeTimes.begin(), lifeTimes.end(), chartTime + 0.0001f);
		const int idx = static_cast<int>(std::distance(lifeTimes.begin(), it)) - 1;
		if (idx < 0 || idx >= (int)drawData.hudLives.size())
			return;

		const float t = chartTime - lifeTimes[idx];
		if (t < 0.f || t > POPUP_TOTAL)
			return;

		const int prevLife =
		    (idx > 0) ? drawData.hudLives[idx - 1] : static_cast<int>(LIFE_DEFAULT);
		const int gained = std::max(0, drawData.hudLives[idx] - prevLife);
		if (gained <= 0)
			return;

		auto easeOut = [](float x) { return 1.f - (1.f - x) * (1.f - x) * (1.f - x); };

		float alpha = 1.f;
		float slideOffset = 0.f;
		if (t < POPUP_SLIDE_IN)
		{
			const float p = easeOut(t / POPUP_SLIDE_IN);
			alpha = p;
			slideOffset = (1.f - p) * 40.f;
		}
		else if (t > POPUP_TOTAL - POPUP_SLIDE_OUT)
		{
			const float p = (t - (POPUP_TOTAL - POPUP_SLIDE_OUT)) / POPUP_SLIDE_OUT;
			alpha = 1.f - easeOut(p);
		}

		const Texture* banner = getHudTexture("skilleffect", "lifeup.png");
		if (!banner)
			return;

		const float uiScale = std::min(size.x / 1920.f, size.y / 1080.f);
		const float offsetX = (size.x - 1920.f * uiScale) * 0.5f;
		auto px = [&](float x) { return position.x + offsetX + x * uiScale; };
		auto tpy = [&](float y) { return position.y + y * uiScale; };
		auto ps = [&](float v) { return v * uiScale; };

		constexpr float SKILL_POPUP_X = 36.f;
		constexpr float SKILL_POPUP_Y = 225.f;
		constexpr float SKILL_POPUP_H = 140.f;
		const float bannerW =
		    SKILL_POPUP_H * ((float)banner->getWidth() / (float)banner->getHeight());

		const float bannerX = SKILL_POPUP_X - slideOffset;
		const float bannerY = SKILL_POPUP_Y;

		drawHudImage(drawList, *banner, px(bannerX), tpy(bannerY), ps(bannerW), ps(SKILL_POPUP_H),
		             alpha);

		const std::string gainText = std::to_string(gained);
		const ImU32 tintColor = IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.f));

		constexpr float DIGIT_OFFSET_X = 0.73f;
		constexpr float DIGIT_OFFSET_Y = 0.68f;
		constexpr float DIGIT_H = 0.26f;

		const float digitH = ps(SKILL_POPUP_H * DIGIT_H);
		float digitX = px(bannerX + bannerW * DIGIT_OFFSET_X);
		const float digitY = tpy(bannerY + SKILL_POPUP_H * DIGIT_OFFSET_Y);

		for (char ch : gainText)
		{
			const Texture* digit = getHudTexture("life", std::string("digit\\") + ch + ".png");
			if (!digit)
				continue;
			const float digitW = digitH * ((float)digit->getWidth() / (float)digit->getHeight());
			drawList->AddImage((ImTextureID)(size_t)digit->getID(), ImVec2(digitX, digitY),
			                   ImVec2(digitX + digitW, digitY + digitH), ImVec2(0, 0), ImVec2(1, 1),
			                   tintColor);
			digitX += digitW;
		}
	}

	static void drawSkillScorePopup(const ScoreContext& context, ImDrawList* drawList,
	                                ImVec2 position, ImVec2 size)
	{
		if (!drawList || size.x <= 1.f || size.y <= 1.f)
			return;
		if (!context.score.metadata.isExtendedScore)
			return;

		const auto& drawData = context.scorePreviewDrawData;
		const auto& lifeTimes = drawData.lifeTimes;
		if (lifeTimes.empty())
			return;

		constexpr float POPUP_SLIDE_IN = 0.18f;
		constexpr float POPUP_SLIDE_OUT = 0.22f;
		constexpr float POPUP_TOTAL = 2.0f;

		const float chartTime = static_cast<float>(context.getTimeAtCurrentTick());

		const auto it = std::upper_bound(lifeTimes.begin(), lifeTimes.end(), chartTime + 0.0001f);
		const int idx = static_cast<int>(std::distance(lifeTimes.begin(), it)) - 1;
		if (idx < 0 || idx >= (int)drawData.hudSkillEffects.size())
			return;

		if (drawData.hudSkillEffects[idx] != static_cast<uint8_t>(SkillEffect::Score))
			return;

		const float t = chartTime - lifeTimes[idx];
		if (t < 0.f || t > POPUP_TOTAL)
			return;

		auto easeOut = [](float x) { return 1.f - (1.f - x) * (1.f - x) * (1.f - x); };

		float alpha = 1.f;
		float slideOffset = 0.f;
		if (t < POPUP_SLIDE_IN)
		{
			const float p = easeOut(t / POPUP_SLIDE_IN);
			alpha = p;
			slideOffset = (1.f - p) * 40.f;
		}
		else if (t > POPUP_TOTAL - POPUP_SLIDE_OUT)
		{
			const float p = (t - (POPUP_TOTAL - POPUP_SLIDE_OUT)) / POPUP_SLIDE_OUT;
			alpha = 1.f - easeOut(p);
		}

		const Texture* banner = getHudTexture("skilleffect", "scoreup.png");
		if (!banner)
			return;

		const float uiScale = std::min(size.x / 1920.f, size.y / 1080.f);
		const float offsetX = (size.x - 1920.f * uiScale) * 0.5f;
		auto px = [&](float x) { return position.x + offsetX + x * uiScale; };
		auto tpy = [&](float y) { return position.y + y * uiScale; };
		auto ps = [&](float v) { return v * uiScale; };

		constexpr float SKILL_POPUP_X = 36.f;
		constexpr float SKILL_POPUP_Y = 225.f;
		constexpr float SKILL_POPUP_H = 140.f;
		const float bannerW =
		    SKILL_POPUP_H * ((float)banner->getWidth() / (float)banner->getHeight());

		const float bannerX = SKILL_POPUP_X - slideOffset;
		const float bannerY = SKILL_POPUP_Y;

		drawHudImage(drawList, *banner, px(bannerX), tpy(bannerY), ps(bannerW), ps(SKILL_POPUP_H),
		             alpha);

		const uint8_t level =
		    (idx < (int)drawData.hudSkillLevels.size()) ? drawData.hudSkillLevels[idx] : 1;

		int boostPct = 60;
		switch (level)
		{
		case 1:
			boostPct = 60;
			break;
		case 2:
			boostPct = 65;
			break;
		case 3:
			boostPct = 70;
			break;
		case 4:
			boostPct = 80;
			break;
		}

		const std::string pctText = std::to_string(boostPct);
		const ImU32 tintColor = IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.f));

		constexpr float DIGIT_OFFSET_X = 0.73f;
		constexpr float DIGIT_OFFSET_Y = 0.68f;
		constexpr float DIGIT_H = 0.26f;

		const float digitH = ps(SKILL_POPUP_H * DIGIT_H);
		float digitX = px(bannerX + bannerW * DIGIT_OFFSET_X);
		const float digitY = tpy(bannerY + SKILL_POPUP_H * DIGIT_OFFSET_Y);

		for (char ch : pctText)
		{
			const Texture* digit = getHudTexture("life", std::string("digit\\") + ch + ".png");
			if (!digit)
				continue;
			const float digitW = digitH * ((float)digit->getWidth() / (float)digit->getHeight());
			drawList->AddImage((ImTextureID)(size_t)digit->getID(), ImVec2(digitX, digitY),
			                   ImVec2(digitX + digitW, digitY + digitH), ImVec2(0, 0), ImVec2(1, 1),
			                   tintColor);
			digitX += digitW;
		}

		const Texture* pct = getHudTexture("life", "digit\\percentage.png");
		if (pct)
		{
			const float pctW = digitH * ((float)pct->getWidth() / (float)pct->getHeight());
			drawList->AddImage((ImTextureID)(size_t)pct->getID(), ImVec2(digitX, digitY),
			                   ImVec2(digitX + pctW, digitY + digitH), ImVec2(0, 0), ImVec2(1, 1),
			                   tintColor);
		}
	}

	static void drawJudgePopup(const ScoreContext& context, ImDrawList* drawList, ImVec2 position,
	                           ImVec2 size)
	{
		if (!drawList || size.x <= 1.f || size.y <= 1.f)
			return;

		const auto& drawData = context.scorePreviewDrawData;
		const auto& judgeTimes = drawData.hudJudgeTimes;
		if (judgeTimes.empty())
			return;

		constexpr float POPUP_POP = 0.12f;
		constexpr float POPUP_HOLD = 0.10f;
		constexpr float POPUP_FADE = 0.18f;
		constexpr float POPUP_TOTAL = POPUP_POP + POPUP_HOLD + POPUP_FADE;

		const float chartTime = static_cast<float>(context.getTimeAtCurrentTick());

		const auto it = std::upper_bound(judgeTimes.begin(), judgeTimes.end(), chartTime + 0.0001f);
		const int idx = static_cast<int>(std::distance(judgeTimes.begin(), it)) - 1;
		if (idx < 0)
			return;

		const float t = chartTime - judgeTimes[idx];
		if (t < 0.f || t > POPUP_TOTAL)
			return;

		auto easeOut = [](float x) { return 1.f - (1.f - x) * (1.f - x) * (1.f - x); };

		float alpha = 1.f;
		float scale = 1.f;
		if (t < POPUP_POP)
		{
			const float p = easeOut(t / POPUP_POP);
			scale = 1.3f - p * 0.3f;
			alpha = p;
		}
		else if (t > POPUP_POP + POPUP_HOLD)
		{
			const float p = (t - POPUP_POP - POPUP_HOLD) / POPUP_FADE;
			alpha = 1.f - easeOut(p);
		}

		const Texture* judgeTex = getHudTexture("score", "judge\\perfect.png");
		if (!judgeTex)
			return;

		const float uiScale = std::min(size.x / 1920.f, size.y / 1080.f);
		const float offsetX = (size.x - 1920.f * uiScale) * 0.5f;
		auto px = [&](float x) { return position.x + offsetX + x * uiScale; };
		auto tpy = [&](float y) { return position.y + y * uiScale; };
		auto ps = [&](float v) { return v * uiScale; };

		constexpr float JUDGE_CENTER_X = 960.f;
		constexpr float JUDGE_CENTER_Y = 720.f;
		constexpr float JUDGE_BASE_H = 70.f;

		const float h = JUDGE_BASE_H * scale;
		const float w = h * ((float)judgeTex->getWidth() / (float)judgeTex->getHeight());

		drawHudImage(drawList, *judgeTex, px(JUDGE_CENTER_X - w * 0.5f),
		             tpy(JUDGE_CENTER_Y - h * 0.5f), ps(w), ps(h), alpha);
	}

	void ScorePreviewWindow::update(ScoreContext& context, Renderer* renderer)
	{
		bool needsRecalc = false;

		if (context.scorePreviewDrawData.noteSpeed != config.pvNoteSpeed)
			needsRecalc = true;

		if (context.scorePreviewDrawData.cachedBaseLifePoint !=
		        context.score.metadata.baseLifePoint ||
		    context.scorePreviewDrawData.cachedIsExtended != context.score.metadata.isExtendedScore)
			needsRecalc = true;

		if (!needsRecalc && !context.score.notes.empty() &&
		    context.scorePreviewDrawData.drawingNotes.empty())
			needsRecalc = true;

		if (!needsRecalc && !context.scorePreviewDrawData.drawingNotes.empty())
		{
			int checkCount = 0;
			for (const auto& note : context.scorePreviewDrawData.drawingNotes)
			{
				if (context.score.notes.find(note.refID) == context.score.notes.end())
				{
					needsRecalc = true;
					break;
				}
				if (++checkCount > 5)
					break;
			}
		}

		if (needsRecalc)
			context.scorePreviewDrawData.calculateDrawData(context.score);

		bool isWindowActive =
		    !ImGui::IsWindowDocked() ||
		    ImGui::GetCurrentWindow()->TabId == ImGui::GetWindowDockNode()->SelectedTabId;
		if (!isWindowActive)
			return;

		ImVec2 size = ImGui::GetContentRegionAvail() - ImVec2{ this->getScrollbarWidth(), 0 };
		ImVec2 position = ImGui::GetCursorScreenPos();
		ImRect boundaries = ImRect(position, position + size);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(boundaries.Min, boundaries.Max, 0xff202020);

		if (config.drawBackground && background.shouldUpdate(context.workingData.jacket))
			background.update(renderer, context.workingData.jacket);

		if (!context.scorePreviewDrawData.effectView.isInitialized())
			context.scorePreviewDrawData.effectView.init();

		if (playbackState.isPlaying)
			context.scorePreviewDrawData.effectView.update(context);
		else if (playbackState.wasLastFramePlaying)
			context.scorePreviewDrawData.effectView.reset();

		static int shaderId = ResourceManager::getShader("basic2d");
		static int pteShaderId = ResourceManager::getShader("particles");
		if (shaderId == -1 || pteShaderId == -1)
			return;

		Shader* shader = ResourceManager::shaders[shaderId];
		Shader* pteShader = ResourceManager::shaders[pteShaderId];
		shader->use();

		float width = size.x, height = size.y;
		float scaledWidth = Engine::STAGE_TARGET_WIDTH * Engine::STAGE_WIDTH_RATIO;
		float scaledHeight = Engine::STAGE_TARGET_HEIGHT * Engine::STAGE_HEIGHT_RATIO;
		float scrTop = Engine::STAGE_TARGET_HEIGHT * Engine::STAGE_TOP_RATIO;
		Utils::fillRect(Engine::STAGE_TARGET_WIDTH, Engine::STAGE_TARGET_HEIGHT, size.x / size.y,
		                width, height);

		float aspectRatio = width / height;
		scaledAspectRatio = scaledWidth / scaledHeight;

		auto view = DirectX::XMMatrixScaling(scaledWidth, scaledHeight, 1.f) *
		            DirectX::XMMatrixTranslation(0.f, -scrTop, 0.f);
		auto projection = Camera().getOffCenterOrthographicProjection(-width / 2, width / 2,
		                                                              height / 2, -height / 2);
		auto viewProjection = DirectX::XMMatrixMultiply(view, projection);
		const auto pView = noteEffectsCamera.getViewMatrix();
		auto pProjection = noteEffectsCamera.getProjectionMatrix(aspectRatio, 0.3f, 1000.f);
		float projectionScale = std::min(aspectRatio / EFFECTS_TARGET_ASPECT, 1.f);
		pProjection =
		    DirectX::XMMatrixScaling(projectionScale, -projectionScale, 1.f) * pProjection;

		shader->setMatrix4("projection", viewProjection);
		float currentTime = context.getTimeAtCurrentTick();

		if (previewBuffer.getWidth() != (unsigned int)size.x ||
		    previewBuffer.getHeight() != (unsigned int)size.y)
			previewBuffer.resize((unsigned int)size.x, (unsigned int)size.y);
		previewBuffer.bind();
		previewBuffer.clear();

		renderer->beginBatch();
		if (config.drawBackground)
		{
			background.setBrightness(config.pvBackgroundBrightness);
			background.draw(renderer, width, height);
		}
		if (context.score.stages.empty())
			drawStage(renderer);
		else
			drawDynamicStage(renderer, context);
		renderer->endBatch();

		context.scorePreviewDrawData.effectView.updateEffects(context, noteEffectsCamera,
		                                                      currentTime);

		shader->use();
		shader->setMatrix4("projection", viewProjection);
		renderer->beginBatch();
		drawLines(context, renderer);
		drawHoldCurves(context, renderer);
		if (config.pvStageCover != 0)
		{
			drawStageCoverMask(renderer);
			renderer->endBatchWithDepthTest(GL_LEQUAL);
		}
		else
			renderer->endBatch();

		pteShader->use();
		pteShader->setMatrix4("projection", pProjection);
		pteShader->setMatrix4("view", pView);
		renderer->beginBatch();
		context.scorePreviewDrawData.effectView.drawUnderNoteEffects(renderer, currentTime);
		renderer->endBatchWithBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
		                               GL_ONE_MINUS_SRC_ALPHA);

		shader->use();
		shader->setMatrix4("projection", viewProjection);
		renderer->beginBatch();

		drawHoldTicks(context, renderer);
		drawNotes(context, renderer);
		if (config.pvStageCover != 0)
		{
			drawStageCoverMask(renderer);
			drawStageCover(renderer);
			drawStageCoverDecoration(renderer);
			renderer->endBatchWithDepthTest(GL_LEQUAL);
		}
		else
			renderer->endBatch();

		pteShader->use();
		pteShader->setMatrix4("projection", pProjection);
		pteShader->setMatrix4("view", pView);
		renderer->beginBatch();
		context.scorePreviewDrawData.effectView.drawEffects(renderer, currentTime);
		renderer->endBatchWithBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
		                               GL_ONE_MINUS_SRC_ALPHA);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		drawList->AddImage((ImTextureID)(size_t)previewBuffer.getTexture(), position,
		                   position + size, { 0, 0 }, { 1, 1 });
		drawComboOverlay(context, drawList, position, size);
		drawHudOverlay(context, drawList, position, size);
		drawSkillLifePopup(context, drawList, position, size);
		drawSkillScorePopup(context, drawList, position, size);
		drawJudgePopup(context, drawList, position, size);
	}

	void ScorePreviewWindow::updateUI(ScoreEditorTimeline& timeline, ScoreContext& context)
	{
		updateToolbar(timeline, context);
		ImGuiIO io = ImGui::GetIO();
		float mouseWheel = io.MouseWheel * 1;
		if (!timeline.isPlaying() && ImGui::IsWindowHovered() && mouseWheel != 0)
			context.currentTick +=
			    std::max(mouseWheel * TICKS_PER_BEAT / 2, (float)-context.currentTick);
		updateScrollbar(timeline, context);

		playbackState.wasLastFramePlaying = playbackState.isPlaying;
		playbackState.isPlaying = timeline.isPlaying();

		if (isFullWindow())
		{
			if (ImGui::BeginPopupContextWindow("preview_context_menu", 1))
			{
				bool _fullWindow = fullWindow;
				if (ImGui::MenuItem(getString("fullscreen_preview"), NULL, &_fullWindow))
					setFullWindow(_fullWindow);

				ImGui::MenuItem(getString("preview_draw_toolbar"), NULL, &config.pvDrawToolbar);
				ImGui::MenuItem(getString("preview_draw_score_hud"), NULL, &config.pvDrawScoreHud);
				ImGui::MenuItem(getString("preview_draw_life_hud"), NULL, &config.pvDrawLifeHud);
				ImGui::MenuItem(getString("return_to_last_tick"), NULL,
				                &config.returnToLastSelectedTickOnPause);
				ImGui::EndPopup();
			}
		}
	}

	void ScorePreviewWindow::setFullWindow(bool _fullWindow) { fullWindow = _fullWindow; }

	void ScorePreviewWindow::loadNoteEffects(Effect::EffectView& effectView)
	{
		int oldProfile = config.pvEffectsProfile == 1 ? 0 : 1;
		const std::string oldEffectsDir =
		    Application::getAppDir() + "res\\effect\\" + std::to_string(oldProfile) + "\\";
		const std::string effectsDir = Application::getAppDir() + "res\\effect\\" +
		                               std::to_string(config.pvEffectsProfile) + "\\";
		size_t effectCount = arrayLength(Effect::effectNames);

		ResourceManager::removeAllParticleEffects();
		int texIndex =
		    ResourceManager::getTextureByFilename(oldEffectsDir + "tex_note_common_all_v2.png");
		if (texIndex > -1)
			ResourceManager::disposeTexture(ResourceManager::textures[texIndex].getID());

		ResourceManager::loadTexture(effectsDir + "tex_note_common_all_v2.png");

		std::vector<std::string> failedParticleFiles;
		for (size_t i = 0; i < effectCount; i++)
		{
			const std::string filename{ effectsDir + Effect::effectNames[i] + ".json" };
			int particleId = ResourceManager::loadParticleEffect(filename);
			if (particleId == -1)
				failedParticleFiles.push_back(filename);
		}

		if (!failedParticleFiles.empty())
		{
			std::string fullErrorMessage = "Failed to load the following note effects: \n\n";
			for (const auto& error : failedParticleFiles)
				fullErrorMessage.append(error).append("\n");

			IO::messageBox(APP_NAME, fullErrorMessage, IO::MessageBoxButtons::Ok,
			               IO::MessageBoxIcon::Warning, Application::windowState.windowHandle);
		}

		effectView.reset();
		effectView.init();
	}

	const Texture& ScorePreviewWindow::getNoteTexture()
	{
		return ResourceManager::textures[noteTextures.notes];
	}

	struct StageRenderProps
	{
		float left = 0;
		float size = 12;
		float pivotLane = 0;
		int divisionSize = 2;
		bool divisionParityOdd = false;
		GuideColor judgeColorA = GuideColor::Purple;
		GuideColor judgeColorB = GuideColor::Purple;
		float styleBlend = 0.f;
		StageBorderStyle leftBorder = StageBorderStyle::Default;
		StageBorderStyle rightBorder = StageBorderStyle::Default;
		float alpha = 1;
		float laneAlpha = 0;
		float judgeLineAlpha = 1;
		float yOffset = 0;
	};

	template <typename T>
	static const T* findActiveAndNext(const std::unordered_map<id_t, T>& changes, id_t stageID,
	                                  int tick, const T*& next)
	{
		const T* active = nullptr;
		next = nullptr;
		for (const auto& [_, change] : changes)
		{
			if (change.stageID != stageID)
				continue;
			if (change.tick <= tick)
			{
				if (!active || change.tick >= active->tick)
					active = &change;
			}
			else if (!next || change.tick < next->tick)
			{
				next = &change;
			}
		}
		return active;
	}

	static StageRenderProps getStagePropsAt(const Score& score, id_t stageID, int tick)
	{
		StageRenderProps props;

		const StageMaskChangeEvent* nextMask = nullptr;
		const StageMaskChangeEvent* mask =
		    findActiveAndNext(score.stageMaskChanges, stageID, tick, nextMask);
		if (mask)
		{
			float t = nextMask ? getEaseFunction(mask->ease)(
			                         0.f, 1.f,
			                         std::clamp((float)(tick - mask->tick) /
			                                        std::max(1, nextMask->tick - mask->tick),
			                                    0.f, 1.f))
			                   : 0.f;
			props.left = nextMask ? lerp(mask->left, nextMask->left, t) : mask->left;
			props.size = nextMask ? lerp(mask->size, nextMask->size, t) : mask->size;
		}

		const StagePivotChangeEvent* nextPivot = nullptr;
		const StagePivotChangeEvent* pivot =
		    findActiveAndNext(score.stagePivotChanges, stageID, tick, nextPivot);
		if (pivot)
		{
			float t = nextPivot ? getEaseFunction(pivot->ease)(
			                          0.f, 1.f,
			                          std::clamp((float)(tick - pivot->tick) /
			                                         std::max(1, nextPivot->tick - pivot->tick),
			                                     0.f, 1.f))
			                    : 0.f;
			props.pivotLane = nextPivot ? lerp(pivot->lane, nextPivot->lane, t) : pivot->lane;
			props.divisionSize = pivot->divisionSize;
			props.divisionParityOdd = pivot->divisionParityOdd;
			props.yOffset =
			    nextPivot ? lerp(pivot->yOffset, nextPivot->yOffset, t) : pivot->yOffset;
		}

		const StageStyleChangeEvent* nextStyle = nullptr;
		const StageStyleChangeEvent* style =
		    findActiveAndNext(score.stageStyleChanges, stageID, tick, nextStyle);
		if (style)
		{
			float t = nextStyle ? getEaseFunction(style->ease)(
			                          0.f, 1.f,
			                          std::clamp((float)(tick - style->tick) /
			                                         std::max(1, nextStyle->tick - style->tick),
			                                     0.f, 1.f))
			                    : 0.f;
			props.judgeColorA = style->judgeLineColor;
			props.judgeColorB = nextStyle ? nextStyle->judgeLineColor : style->judgeLineColor;
			props.styleBlend = nextStyle ? t : 0.f;
			props.leftBorder = style->leftBorderStyle;
			props.rightBorder = style->rightBorderStyle;
			props.alpha = nextStyle ? lerp(style->alpha, nextStyle->alpha, t) : style->alpha;
			props.laneAlpha =
			    nextStyle ? lerp(style->laneAlpha, nextStyle->laneAlpha, t) : style->laneAlpha;
			props.judgeLineAlpha = nextStyle
			                           ? lerp(style->judgeLineAlpha, nextStyle->judgeLineAlpha, t)
			                           : style->judgeLineAlpha;
		}

		return props;
	}

	static const CameraChangeEvent* findActiveAndNextCamera(const Score& score, int tick,
	                                                        const CameraChangeEvent*& next)
	{
		const CameraChangeEvent* active = nullptr;
		next = nullptr;
		for (const auto& [_, change] : score.cameraChanges)
		{
			if (change.tick <= tick)
			{
				if (!active || change.tick >= active->tick)
					active = &change;
			}
			else if (!next || change.tick < next->tick)
			{
				next = &change;
			}
		}
		return active;
	}

	static float cameraTiltedWidth(float depth, float stageTilt)
	{
		stageTilt = std::clamp(stageTilt, 0.f, 1.f);
		const float mid = (powf(1.06f, -45.f) + 1.f) * 0.5f;
		return stageTilt * depth + (1.f - stageTilt) * mid;
	}

	// this is the beginning of the real engine approach-at-tilt port
	constexpr float APPROACH_SCALE_CONST = 0.07414833f;
	constexpr float APPROACH_TILT_LERP_MIN = 0.05f;

	static float approachCurveBase(float x) { return powf(APPROACH_SCALE_CONST, 1.f - x); }

	static float inverseApproachCurveBase(float v)
	{
		v = std::max(0.000001f, v);
		return 1.f - logf(v) / logf(APPROACH_SCALE_CONST);
	}

	static float remapRange(float a0, float a1, float b0, float b1, float v)
	{
		if (std::abs(a1 - a0) < 0.000001f)
			return b0;
		return b0 + (b1 - b0) * (v - a0) / (a1 - a0);
	}

	static void approachSliceWindow(float stageTilt, float spawnDepth, float& outStart,
	                                float& outSliceSpawn)
	{
		float wJudge = cameraTiltedWidth(1.f, stageTilt);
		float spawnFraction = stageTilt * (1.f - spawnDepth) / std::max(0.000001f, wJudge);
		outSliceSpawn = 1.f - spawnFraction;
		outStart = inverseApproachCurveBase(outSliceSpawn);
	}

	static float approachSlice(float progress, float stageTilt, float spawnDepth)
	{
		float start, sliceSpawn;
		approachSliceWindow(stageTilt, spawnDepth, start, sliceSpawn);
		float travel = approachCurveBase(lerp(start, 1.f, progress));
		return remapRange(sliceSpawn, 1.f, spawnDepth, 1.f, travel);
	}

	static float approachAtTilt(float progress, float stageTilt)
	{
		stageTilt = std::clamp(stageTilt, 0.f, 1.f);
		if (stageTilt >= 1.f)
			return approachCurveBase(progress);

		const float spawnDepth = APPROACH_SCALE_CONST;
		if (stageTilt <= 0.f)
			return lerp(spawnDepth, 1.f, progress);

		if (stageTilt < APPROACH_TILT_LERP_MIN)
		{
			float linear = lerp(spawnDepth, 1.f, progress);
			float sliceAtFloor = approachSlice(progress, APPROACH_TILT_LERP_MIN, spawnDepth);
			return lerp(linear, sliceAtFloor, stageTilt / APPROACH_TILT_LERP_MIN);
		}

		return approachSlice(progress, stageTilt, spawnDepth);
	}

	static CameraRenderProps getCameraPropsAt(const Score& score, int tick)
	{
		CameraRenderProps props;

		const CameraChangeEvent* nextCam = nullptr;
		const CameraChangeEvent* cam = findActiveAndNextCamera(score, tick, nextCam);
		if (!cam)
			return props;

		float t =
		    nextCam
		        ? getEaseFunction(cam->ease)(
		              0.f, 1.f,
		              std::clamp((float)(tick - cam->tick) / std::max(1, nextCam->tick - cam->tick),
		                         0.f, 1.f))
		        : 0.f;

		float position = nextCam ? lerp(cam->left, nextCam->left, t) : cam->left;
		float size = std::max(0.0001f, nextCam ? lerp(cam->size, nextCam->size, t) : cam->size);
		float zoomTargetLane =
		    nextCam ? lerp(cam->zoomTargetLane, nextCam->zoomTargetLane, t) : cam->zoomTargetLane;
		float zoomTargetY =
		    nextCam ? lerp(cam->zoomTargetY, nextCam->zoomTargetY, t) : cam->zoomTargetY;
		float stageTilt = std::clamp(
		    nextCam ? lerp(cam->stageTilt, nextCam->stageTilt, t) : cam->stageTilt, 0.f, 1.f);
		float zoom = std::max(0.01f, nextCam ? lerp(cam->zoom, nextCam->zoom, t) : cam->zoom);
		float rotateDeg = nextCam ? lerp(cam->rotate, nextCam->rotate, t) : cam->rotate;
		StageZoomVerticalAlign align =
		    (nextCam && t >= 0.5f) ? nextCam->zoomVerticalAlign : cam->zoomVerticalAlign;

		float halfSize = size * 0.5f;
		float lane = (position - 6.0f) + halfSize;
		float scale = 6.0f / std::max(0.0001f, halfSize);

		float targetTravel = approachAtTilt(1.f - zoomTargetY, stageTilt);
		float targetWidth = cameraTiltedWidth(targetTravel, stageTilt);
		float targetX = ((lane + zoomTargetLane) * targetWidth - lane) * scale;

		const float stageTopY = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const float stageBottomY = stageTopY + Engine::STAGE_TEX_HEIGHT / Engine::STAGE_LANE_HEIGHT;

		props.lane = lane;
		props.scale = scale;
		props.zoom = zoom;
		props.zoomTargetX = targetX;
		props.zoomTargetTravel = targetTravel;
		props.zoomAnchorY =
		    align == StageZoomVerticalAlign::Center ? (stageTopY + stageBottomY) * 0.5f : 1.f;
		props.rotate = rotateDeg * (3.14159265f / 180.f);
		props.stageTilt = stageTilt;
		return props;
	}

	constexpr float CAMERA_ASPECT_CORRECTION =
	    (Engine::STAGE_TARGET_WIDTH * Engine::STAGE_WIDTH_RATIO) /
	    (Engine::STAGE_TARGET_HEIGHT * Engine::STAGE_HEIGHT_RATIO);

	constexpr float CAMERA_ROTATE_PIVOT_Y =
	    (Engine::STAGE_LANE_TOP + Engine::STAGE_TEX_HEIGHT / 2.f) / Engine::STAGE_LANE_HEIGHT -
	    0.06f / Engine::STAGE_HEIGHT_RATIO;

	static void applyCameraPoint(float& x, float& y, const CameraRenderProps& camera)
	{
		float baseX = (x - camera.lane) * camera.scale;
		float tx = camera.zoom * (baseX - camera.zoomTargetX);
		float ty = camera.zoom * (y - camera.zoomTargetTravel) + camera.zoomAnchorY;

		if (std::abs(camera.rotate) > 0.000001f)
		{
			float c = cosf(camera.rotate), s = sinf(camera.rotate);
			float px = tx * CAMERA_ASPECT_CORRECTION;
			float py = ty - CAMERA_ROTATE_PIVOT_Y;
			tx = (px * c - py * s) / CAMERA_ASPECT_CORRECTION;
			ty = (px * s + py * c) + CAMERA_ROTATE_PIVOT_Y;
		}

		x = tx;
		y = ty;
	}

	static void applyCameraTilt(float& x, float& y, const CameraRenderProps& camera)
	{
		float untiltedWidth = std::abs(y) > 0.000001f ? y : 1.f;
		float lane = x / untiltedWidth;
		x = lane * cameraTiltedWidth(y, camera.stageTilt);
		applyCameraPoint(x, y, camera);
	}

	// this is the beginning of the icon camera projection
	// refY must be the vertex's Y BEFORE any icon-shape SpriteTransform is applied (i.e. the
	// flat near-judgeline offset). Using the post-transform Y here (like the arrow/diamond's
	// baked-in shape skew) makes each corner reproject to a different depth and shears the icon.
	static void applyCameraTiltIcon(float& x, float& y, float refY, float baseTravel,
	                                const CameraRenderProps& camera)
	{
		float stageTilt = std::clamp(camera.stageTilt, 0.f, 1.f);
		float lineY = std::abs(refY) > 0.000001f ? refY : 1.f;
		float lane = x / lineY;
		float depth = baseTravel + (lineY - 1.f) * lerp(1.f, baseTravel, stageTilt);
		x = lane * cameraTiltedWidth(depth, stageTilt);
		y = depth;
		applyCameraPoint(x, y, camera);
	}

	// Sonolus-accurate icon billboard projection (matches `layout_tick` / `layout_flick_arrow`
	// in the reference engine, e.g. sonolus-next-rush-engine's sekai/lib/layout.py).
	//
	// Unlike the note body strip, icons (flick arrows, hold ticks, trace diamonds) do NOT recede
	// in perspective across their own height in the real engine — they're flat billboards
	// anchored at a single depth. The reference engine builds them by projecting two points at
	// the SAME travel (the icon's left/right edge) to get a screen-space width vector, then
	// rotating that vector 90 degrees to get the "up" (height) direction, so the icon keeps a
	// consistent, unsheared shape under any stage tilt/rotation.
	//
	// applyCameraTiltIcon (above) instead re-derives a *different* depth per vertex from that
	// vertex's own raw Y, which shears the icon into a trapezoid as tilt decreases -- that's the
	// bug being fixed here. rawX/rawY are the vertex position from the local (pre-camera) shape,
	// centered around baseline Y = 1; center/travel place that local shape's anchor in world
	// space.
	static void applyCameraTiltIconBillboard(float& x, float& y, float rawX, float rawY,
	                                         float center, float travel,
	                                         const CameraRenderProps& camera)
	{
		// Project the icon's anchor, and a point one lane-unit to its right, both at the same
		// depth -- this gives the screen-space direction+length of one lane-unit of width once
		// tilt, zoom, pan and rotation have all been applied.
		float cx = center * travel, cy = travel;
		applyCameraTilt(cx, cy, camera);

		float rx = (center + 1.f) * travel, ry = travel;
		applyCameraTilt(rx, ry, camera);

		float dirX = rx - cx, dirY = ry - cy;
		float perpX = -dirY, perpY = dirX; // 90-degree rotation of the width vector

		float localX = rawX, localY = rawY - 1.f; // offsets around the icon's own baseline
		x = cx + localX * dirX + localY * perpX;
		y = cy + localX * dirY + localY * perpY;
	}

	constexpr int SPR_DS_LANE_BACKGROUND = 0;
	constexpr int SPR_DS_LANE_DIVIDER = 1;
	constexpr int SPR_DS_STAGE_BORDER = 2;
	constexpr int SPR_DS_JUDGE_BACKGROUND = 3;
	constexpr int SPR_DS_JUDGE_CENTER_BLACK = 4;
	constexpr int SPR_DS_JUDGE_CENTER_BLUE = 5;
	constexpr int SPR_DS_JUDGE_CENTER_CYAN = 6;
	constexpr int SPR_DS_JUDGE_CENTER_GREEN = 7;
	constexpr int SPR_DS_JUDGE_CENTER_NEUTRAL = 8;
	constexpr int SPR_DS_JUDGE_CENTER_PURPLE = 9;
	constexpr int SPR_DS_JUDGE_CENTER_RED = 10;
	constexpr int SPR_DS_JUDGE_CENTER_YELLOW = 11;
	constexpr int SPR_DS_JUDGE_EDGE_BLACK = 12;
	constexpr int SPR_DS_JUDGE_EDGE_BLUE = 13;
	constexpr int SPR_DS_JUDGE_EDGE_CYAN = 14;
	constexpr int SPR_DS_JUDGE_EDGE_GREEN = 15;
	constexpr int SPR_DS_JUDGE_EDGE_NEUTRAL = 16;
	constexpr int SPR_DS_JUDGE_EDGE_PURPLE = 17;
	constexpr int SPR_DS_JUDGE_EDGE_RED = 18;
	constexpr int SPR_DS_JUDGE_EDGE_YELLOW = 19;
	constexpr int SPR_DS_JUDGE_GRADIENT_BLACK = 20;
	constexpr int SPR_DS_JUDGE_GRADIENT_BLUE = 21;
	constexpr int SPR_DS_JUDGE_GRADIENT_CYAN = 22;
	constexpr int SPR_DS_JUDGE_GRADIENT_GREEN = 23;
	constexpr int SPR_DS_JUDGE_GRADIENT_NEUTRAL = 24;
	constexpr int SPR_DS_JUDGE_GRADIENT_PURPLE = 25;
	constexpr int SPR_DS_JUDGE_GRADIENT_RED = 26;
	constexpr int SPR_DS_JUDGE_GRADIENT_YELLOW = 27;
	constexpr float DS_DIVIDER_WIDTH = 0.045f;
	constexpr float DS_BORDER_WIDTH = 0.09f;
	constexpr int guideColorToDsIndex[8] = { 4, 6, 3, 1, 7, 5, 2, 0 };

	static std::array<DirectX::XMFLOAT4, 4> dsPerspectiveUV(const Sprite& spr, const Texture& tex)
	{
		float texelX = 0.5f / tex.getWidth();
		float texelY = 0.5f / tex.getHeight();
		float u0 = spr.getX() / tex.getWidth() + texelX;
		float u1 = (spr.getX() + spr.getWidth()) / tex.getWidth() - texelX;
		float v0 = spr.getY() / tex.getHeight() + texelY;
		float v1 = (spr.getY() + spr.getHeight()) / tex.getHeight() - texelY;
		return { { { u1, v0, 0.f, 1.f },
			       { u1, v1, 0.f, 1.f },
			       { u0, v1, 0.f, 1.f },
			       { u0, v0, 0.f, 1.f } } };
	}

	static void dsPushSprite(Renderer* renderer, const Texture& tex, int sprIndex, float left,
	                         float right, float top, float bottom, float alpha, int z,
	                         const Score& score, int tick)
	{
		if (alpha <= 0.001f || !isArrayIndexInBounds(sprIndex, tex.sprites))
			return;
		const Sprite& spr = tex.sprites[sprIndex];
		auto vPos = Engine::perspectiveQuadvPos(left, right, top, bottom);
		if (!score.cameraChanges.empty())
		{
			CameraRenderProps camera = getCameraPropsAt(score, tick);
			for (auto& v : vPos)
				applyCameraTilt(v.x, v.y, camera);
		}
		auto uv = dsPerspectiveUV(spr, tex);
		renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint, alpha),
		                   (int)tex.getID(), z);
	}

	void ScorePreviewWindow::drawDynamicStage(Renderer* renderer, ScoreContext& context)
	{
		int index = ResourceManager::getTexture("dynamic_stage");
		if (index == -1)
			return;
		const Texture& tex = ResourceManager::textures[index];

		const float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const float stageBottom = stageTop + Engine::STAGE_TEX_HEIGHT / Engine::STAGE_LANE_HEIGHT;
		const float judgeHalf = 75.f / 850.f / 2.f;
		const float judgeTop = 1.f - judgeHalf;
		const float judgeBottom = 1.f + judgeHalf;

		float bgMaxAlpha = 0.f;
		float bgLeft = FLT_MAX, bgRight = -FLT_MAX;

		for (const auto& [stageID, stage] : context.score.stages)
		{
			StageRenderProps props = getStagePropsAt(context.score, stageID, context.currentTick);
			if (props.alpha <= 0.001f)
				continue;

			float travel = (float)Engine::approachProgress(1.f - props.yOffset);
			float curJudgeTop = judgeTop * travel;
			float curJudgeBottom = judgeBottom * travel;

			float laneLeft = props.left - 6.0f;
			float laneRight = laneLeft + props.size;
			float laneAlpha = props.alpha * props.laneAlpha * config.pvStageOpacity;
			float judgeAlpha = props.alpha * props.judgeLineAlpha;

			bgMaxAlpha = std::max(bgMaxAlpha, laneAlpha);
			bgLeft = std::min(bgLeft, laneLeft);
			bgRight = std::max(bgRight, laneRight);

			if (laneAlpha > 0.001f && props.divisionSize > 0)
			{
				float offset =
				    props.pivotLane + (props.divisionParityOdd ? props.divisionSize * 0.5f : 0.f);
				int kStart = (int)std::floor((laneLeft - offset) / props.divisionSize) + 1;
				int kEnd = (int)std::ceil((laneRight - offset) / props.divisionSize) - 1;
				for (int k = kStart; k <= kEnd; ++k)
				{
					float lane = offset + k * props.divisionSize;
					if (lane > laneLeft + 0.01f && lane < laneRight - 0.01f)
						dsPushSprite(renderer, tex, SPR_DS_LANE_DIVIDER,
						             lane - DS_DIVIDER_WIDTH * 0.5f, lane + DS_DIVIDER_WIDTH * 0.5f,
						             stageTop, stageBottom, laneAlpha, -1, context.score,
						             context.currentTick);
				}
			}

			auto pushBorder = [&](StageBorderStyle style, float edge, float inward)
			{
				if (style == StageBorderStyle::Disabled)
					return;
				float width = style == StageBorderStyle::Default  ? DS_BORDER_WIDTH
				              : style == StageBorderStyle::Medium ? DS_BORDER_WIDTH * 0.55f
				                                                  : DS_BORDER_WIDTH * 0.35f;
				float alphaScale = style == StageBorderStyle::Default  ? 1.f
				                   : style == StageBorderStyle::Medium ? 0.85f
				                                                       : 0.6f;
				dsPushSprite(renderer, tex, SPR_DS_STAGE_BORDER, edge, edge + width * inward,
				             stageTop, stageBottom, laneAlpha * alphaScale, -1, context.score,
				             context.currentTick);
			};
			pushBorder(props.leftBorder, laneLeft, 1.f);
			pushBorder(props.rightBorder, laneRight, -1.f);

			int colorIdxA = guideColorToDsIndex[(int)props.judgeColorA];
			int colorIdxB = guideColorToDsIndex[(int)props.judgeColorB];
			constexpr float DS_JUDGE_EDGE_WIDTH = 0.12f;

			auto drawJudgeColor = [&](int colorIdx, float blend)
			{
				if (blend <= 0.001f)
					return;
				float a = judgeAlpha * blend;
				const float middleTopG = curJudgeTop + (curJudgeBottom - curJudgeTop) / 8.f;
				const float middleBottomG = curJudgeBottom - (curJudgeBottom - curJudgeTop) / 8.f;
				const float centerLane = (laneLeft + laneRight) * 0.5f;

				dsPushSprite(renderer, tex, SPR_DS_JUDGE_GRADIENT_BLACK + colorIdx, laneLeft,
				             centerLane, middleBottomG, curJudgeBottom, a, 0, context.score,
				             context.currentTick);
				dsPushSprite(renderer, tex, SPR_DS_JUDGE_GRADIENT_BLACK + colorIdx, laneRight,
				             centerLane, middleBottomG, curJudgeBottom, a, 0, context.score,
				             context.currentTick);
				dsPushSprite(renderer, tex, SPR_DS_JUDGE_GRADIENT_BLACK + colorIdx, laneLeft,
				             centerLane, curJudgeTop, middleTopG, a, 0, context.score,
				             context.currentTick);
				dsPushSprite(renderer, tex, SPR_DS_JUDGE_GRADIENT_BLACK + colorIdx, laneRight,
				             centerLane, curJudgeTop, middleTopG, a, 0, context.score,
				             context.currentTick);

				dsPushSprite(renderer, tex, SPR_DS_JUDGE_EDGE_BLACK + colorIdx, laneLeft,
				             laneLeft + DS_JUDGE_EDGE_WIDTH, curJudgeTop, curJudgeBottom, a, 1,
				             context.score, context.currentTick);
				dsPushSprite(renderer, tex, SPR_DS_JUDGE_EDGE_BLACK + colorIdx,
				             laneRight - DS_JUDGE_EDGE_WIDTH, laneRight, curJudgeTop,
				             curJudgeBottom, a, 1, context.score, context.currentTick);

				if (props.divisionSize > 0)
				{
					float halfOffset = props.divisionParityOdd ? 0.5f : 0.f;
					float shifted = props.pivotLane + halfOffset;
					int kStart = (int)std::floor(laneLeft - shifted + 0.001f) + 1;
					int kEnd = (int)std::ceil(laneRight - shifted - 0.001f) - 1;
					float middleTop = curJudgeTop + (curJudgeBottom - curJudgeTop) / 5.f;
					float middleBottom = curJudgeBottom - (curJudgeBottom - curJudgeTop) / 5.f;
					constexpr float divW = 0.028f;

					for (int k = kStart; k <= kEnd; ++k)
					{
						float lane = shifted + k;
						if (lane > laneLeft + 0.001f && lane < laneRight - 0.001f)
						{
							dsPushSprite(renderer, tex, SPR_DS_JUDGE_CENTER_BLACK + colorIdx,
							             lane - divW * 0.5f, lane + divW * 0.5f, middleTop,
							             middleBottom, a, 1, context.score, context.currentTick);
							dsPushSprite(renderer, tex, SPR_DS_JUDGE_EDGE_BLACK + colorIdx,
							             lane - divW * 0.5f, lane + divW * 0.5f, middleTop,
							             middleBottom, a * 0.5f, 1, context.score,
							             context.currentTick);
						}
					}
				}
			};

			dsPushSprite(renderer, tex, SPR_DS_JUDGE_BACKGROUND, laneLeft, laneRight, curJudgeTop,
			             curJudgeBottom, judgeAlpha * 0.5f, -1, context.score, context.currentTick);
			drawJudgeColor(colorIdxA, 1.f - props.styleBlend);
			drawJudgeColor(colorIdxB, props.styleBlend);
		}

		if (bgMaxAlpha > 0.001f && bgLeft < bgRight)
			dsPushSprite(renderer, tex, SPR_DS_LANE_BACKGROUND, bgLeft, bgRight, stageTop,
			             stageBottom, bgMaxAlpha, -1, context.score, context.currentTick);
	}

	void ScorePreviewWindow::drawStage(Renderer* renderer)
	{
		int index = ResourceManager::getTexture("stage");
		if (index == -1)
			return;
		const Texture& stage = ResourceManager::textures[index];
		if (!isArrayIndexInBounds(SPR_SEKAI_STAGE, stage.sprites))
			return;
		const Sprite& stageSprite = stage.sprites[SPR_SEKAI_STAGE];
		constexpr float stageWidth =
		    (Engine::STAGE_TEX_WIDTH / Engine::STAGE_LANE_WIDTH) * Engine::STAGE_NUM_LANES;
		constexpr float stageLeft = -stageWidth / 2;
		constexpr float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		constexpr float stageHeight = Engine::STAGE_TEX_HEIGHT / Engine::STAGE_LANE_HEIGHT;

		renderer->drawRectangle(Vector2(stageLeft, stageTop), Vector2(stageWidth, stageHeight),
		                        stage, stageSprite.getX(),
		                        stageSprite.getX() + stageSprite.getWidth(), stageSprite.getY(),
		                        stageSprite.getY() + stageSprite.getHeight(),
		                        Color(defaultTint.r, defaultTint.g, defaultTint.b,
		                              defaultTint.a * config.pvStageOpacity),
		                        -1);
	}

	void ScorePreviewWindow::drawStageCoverMask(Renderer* renderer)
	{
		int index = ResourceManager::getTexture("stage");
		if (index == -1)
			return;
		const Texture& stage = ResourceManager::textures[index];
		constexpr float stageWidth =
		    (Engine::STAGE_TEX_WIDTH / Engine::STAGE_LANE_WIDTH) * Engine::STAGE_NUM_LANES;
		constexpr float stageLeft = -stageWidth / 2, stageRight = stageWidth / 2;

		constexpr float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const float stageHeight = config.pvStageCover * (1 - stageTop);

		static auto model = DirectX::XMMatrixTranslation(0, 0, 1);
		auto vPos = Engine::quadvPos(stageLeft, stageRight, stageTop + stageHeight, 0);
		auto uv = Utils::getUV(0.f, 1.f, 0.f, 1.f);
		renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, 0.f), (int)stage.getID(), 0);
	}

	void ScorePreviewWindow::drawStageCover(Renderer* renderer)
	{
		int index = ResourceManager::getTexture("stage");
		if (index == -1)
			return;
		const Texture& stage = ResourceManager::textures[index];
		if (!isArrayIndexInBounds(SPR_SEKAI_STAGE, stage.sprites))
			return;
		const Sprite& stageSprite = stage.sprites[SPR_SEKAI_STAGE];
		constexpr float stageWidth =
		    (Engine::STAGE_TEX_WIDTH / Engine::STAGE_LANE_WIDTH) * Engine::STAGE_NUM_LANES;
		const float stageLeft = -stageWidth / 2, stageRight = stageWidth / 2;

		constexpr float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const float stageHeight = config.pvStageCover * (1 - stageTop);
		const float spriteHeight =
		    config.pvStageCover * (Engine::STAGE_LANE_HEIGHT - Engine::STAGE_LANE_TOP);

		static auto model = DirectX::XMMatrixTranslation(0, 0, 1);

		float texW = (float)stage.getWidth();
		float texH = (float)stage.getHeight();
		auto vPos = Engine::quadvPos(stageLeft, stageRight, stageTop + stageHeight, stageTop);
		auto uv = Utils::getUV(
		    stageSprite.getX() / texW, (stageSprite.getX() + stageSprite.getWidth()) / texW,
		    stageSprite.getY() / texH, (stageSprite.getY() + spriteHeight) / texH);

		renderer->pushQuad(vPos, uv, model, DirectX::XMFLOAT4(0, 0, 0, config.pvStageOpacity),
		                   (int)stage.getID(), 0);
	}

	void ScorePreviewWindow::drawStageCoverDecoration(Renderer* renderer)
	{
		if (noteTextures.notes == -1)
			return;

		constexpr float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const Texture& noteTex = getNoteTexture();
		size_t sprIndex = SPR_SIMULTANEOUS_CONNECTION;
		size_t transIndex = static_cast<size_t>(SpriteType::SimultaneousLine);
		if (!isArrayIndexInBounds(sprIndex, noteTex.sprites))
			return;
		if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms))
			return;

		const SpriteTransform& lineTransform = ResourceManager::spriteTransforms[transIndex];
		const Sprite& sprite = noteTex.sprites[sprIndex];
		float x = 0.12f * (1.f - config.pvStageCover);
		auto vPos = lineTransform.apply(Engine::perspectiveQuadvPos(
		    -6.f - x, 6.f + x, 1.f + Engine::getNoteHeight(), 1.f - Engine::getNoteHeight()));
		float y = stageTop + config.pvStageCover * (1.f - stageTop);
		auto uv = Engine::quadUV(sprite, noteTex);
		auto model = DirectX::XMMatrixScaling(y, y, 1.f);
		renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, config.pvStageOpacity),
		                   (int)noteTex.getID(), -1);
	}

	void ScorePreviewWindow::drawNotes(const ScoreContext& context, Renderer* renderer)
	{
		double current_tm =
		    accumulateDuration(context.currentTick, TICKS_PER_BEAT, context.score.tempoChanges);
		const auto layer_stm = getCurrentLayerScaledTimes(context);

		const auto& drawData = context.scorePreviewDrawData;
		CameraRenderProps camera = getCameraPropsAt(context.score, context.currentTick);

		for (auto& note : drawData.drawingNotes)
		{
			// 修正：at() を find() に変えて、データが存在するかチェックする
			auto it = context.score.notes.find(note.refID);
			if (it == context.score.notes.end())
				continue; // 見つからない場合は描画をスキップ

			const Note& noteData = it->second;

			if (context.currentTick > noteData.tick)
				continue;

			int layer = std::clamp(noteData.layer, 0, (int)context.score.layers.size() - 1);
			double scaled_tm = layer_stm[layer];

			if (scaled_tm < note.visualTime.min)
				continue;

			double progress = unlerpD(note.visualTime.min, note.visualTime.max, scaled_tm);
			double y = approachAtTilt((float)progress, camera.stageTilt);
			float l = Engine::laneToLeft(noteData.lane),
			      r = Engine::laneToLeft(noteData.lane) + noteData.width;

			drawNoteBase(renderer, noteData, l, r, (float)y, 1.f, camera);
			if (noteData.friction)
				drawTraceDiamond(renderer, noteData, l, r, (float)y, camera);
			if (noteData.isFlick())
				drawFlickArrow(renderer, noteData, (float)y, current_tm, camera);
		}
	}

	void ScorePreviewWindow::drawLines(const ScoreContext& context, Renderer* renderer)
	{
		if (!config.pvSimultaneousLine || noteTextures.notes == -1)
			return;

		const auto& drawData = context.scorePreviewDrawData.drawingLines;

		const Texture& texture = getNoteTexture();
		size_t sprIndex = SPR_SIMULTANEOUS_CONNECTION;
		if (!isArrayIndexInBounds(sprIndex, texture.sprites))
			return;
		const Sprite& sprite = texture.sprites[sprIndex];

		// ★ 純正通り SimultaneousLine を使用
		size_t transIndex = static_cast<size_t>(SpriteType::SimultaneousLine);
		if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms))
			return;
		const SpriteTransform& lineTransform = ResourceManager::spriteTransforms[transIndex];
		CameraRenderProps lineCamera = getCameraPropsAt(context.score, context.currentTick);

		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();
		float noteDuration = Engine::getNoteDuration(config.pvNoteSpeed);

		// ★ 純正と完全に同じ計算式 (1 + h と 1 - h) に戻して裏返りを修正
		const float noteTop = 1.0f + Engine::getNoteHeight();
		const float noteBottom = 1.0f - Engine::getNoteHeight();

		for (auto& line : drawData)
		{
			if (context.currentTick > std::max(line.leftTick, line.rightTick))
				continue;

			double left_stm = getCachedLayerScaledTime(context, line.leftTick, line.leftLayer);
			double right_stm = getCachedLayerScaledTime(context, line.rightTick, line.rightLayer);

			double current_left_stm =
			    getCachedLayerScaledTime(context, context.currentTick, line.leftLayer);
			double current_right_stm =
			    getCachedLayerScaledTime(context, context.currentTick, line.rightLayer);

			double left_progress = 1.0 - (left_stm - current_left_stm) / noteDuration;
			double right_progress = 1.0 - (right_stm - current_right_stm) / noteDuration;

			if (left_progress < 0.0 && right_progress < 0.0)
				continue;
			if ((left_progress < 1.0 && 1.0 < right_progress) ||
			    (left_progress > 1.0 && 1.0 > right_progress))
				continue;

			double adj_left_progress = std::max(left_progress, 0.0);
			double adj_right_progress = std::max(right_progress, 0.0);

			float adj_left_lane = line.leftLane;
			float adj_right_lane = line.rightLane;

			if (std::abs(left_progress - right_progress) > 1e-6)
			{
				double adj_left_frac = unlerpD(left_progress, right_progress, adj_left_progress);
				double adj_right_frac = unlerpD(left_progress, right_progress, adj_right_progress);
				adj_left_lane = lerpD(line.leftLane, line.rightLane, adj_left_frac);
				adj_right_lane = lerpD(line.leftLane, line.rightLane, adj_right_frac);
			}

			// this is the beginning of the double-reshape fix (sim line)
			float adj_left_travel = approachAtTilt((float)adj_left_progress, lineCamera.stageTilt);
			float adj_right_travel =
			    approachAtTilt((float)adj_right_progress, lineCamera.stageTilt);

			if (std::abs(adj_left_lane - adj_right_lane) < 1e-6 &&
			    std::abs(adj_left_travel - adj_right_travel) < 1e-6)
				continue;

			if (adj_left_lane > adj_right_lane)
			{
				std::swap(adj_left_lane, adj_right_lane);
				std::swap(adj_left_travel, adj_right_travel);
			}

			// laneToLeft の二重変換を排し、構築済みの物理座標をそのまま使用
			float noteLeft = adj_left_lane;
			float noteRight = adj_right_lane;

			if (config.pvMirrorScore)
			{
				noteLeft *= -1.f;
				noteRight *= -1.f;
				std::swap(noteLeft, noteRight);
				std::swap(adj_left_travel, adj_right_travel);
			}

			// ★ 純正の `auto vPos = lineTransform.apply(Engine::perspectiveQuadvPos(...));`
			// を完全再現
			auto rawPos = Engine::perspectiveQuadvPos(noteLeft, noteRight, noteTop, noteBottom);
			auto vPos = lineTransform.apply(rawPos);

			// ★ 純正の `DirectX::XMMatrixScaling(y, y, 1.f)` を、左右独立して適用
			vPos[0].x *= adj_right_travel;
			vPos[0].y *= adj_right_travel;
			vPos[1].x *= adj_right_travel;
			vPos[1].y *= adj_right_travel;

			vPos[2].x *= adj_left_travel;
			vPos[2].y *= adj_left_travel;
			vPos[3].x *= adj_left_travel;
			vPos[3].y *= adj_left_travel;

			for (auto& v : vPos)
				applyCameraTilt(v.x, v.y, lineCamera);

			auto uv =
			    Utils::getUV(sprite.getX() / texW, (sprite.getX() + sprite.getWidth()) / texW,
			                 sprite.getY() / texH, (sprite.getY() + sprite.getHeight()) / texH);

			// ★ 純正の Z-Index と Tint を完全再現
			float center_y = (adj_left_travel + adj_right_travel) / 2.0f;
			int zIndex = Engine::getZIndex(SpriteLayer::UNDER_NOTE_EFFECT, 0, center_y);

			renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint),
			                   (int)texture.getID(), zIndex);
		}
	}

	void ScorePreviewWindow::drawHoldTicks(const ScoreContext& context, Renderer* renderer)
	{
		if (noteTextures.notes == -1)
			return;
		const auto layer_stm = getCurrentLayerScaledTimes(context);
		CameraRenderProps tickCamera = getCameraPropsAt(context.score, context.currentTick);

		const float notesHeight = Engine::getNoteHeight() * 1.3f;
		const float w = notesHeight / scaledAspectRatio;
		const float noteTop = 1. + notesHeight, noteBottom = 1. - notesHeight;
		const Texture& texture = getNoteTexture();
		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();

		size_t transIndex = static_cast<size_t>(SpriteType::HoldTick);
		if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms))
			return;
		const SpriteTransform& transform = ResourceManager::spriteTransforms[transIndex];

		for (auto& tick : context.scorePreviewDrawData.drawingHoldTicks)
		{
			// 存在チェックを行い、データが既に消えていたら描画を安全にスキップする
			auto it = context.score.notes.find(tick.refID);
			if (it == context.score.notes.end())
				continue;
			const Note& noteData = it->second;

			if (context.currentTick > noteData.tick)
				continue;

			int layer = std::clamp(noteData.layer, 0, (int)context.score.layers.size() - 1);
			double scaled_tm = layer_stm[layer];

			if (scaled_tm < tick.visualTime.min)
				continue;

			float tickProgress =
			    (float)unlerpD(tick.visualTime.min, tick.visualTime.max, scaled_tm);
			float y = approachAtTilt(tickProgress, tickCamera.stageTilt);

			//  Y座標クリッピング
			if (y < -0.1 || y > 1.2)
				continue;

			int sprIndex = getNoteSpriteIndex(noteData);
			if (!isArrayIndexInBounds(sprIndex, texture.sprites))
				continue;
			const Sprite& sprite = texture.sprites[sprIndex];
			const float tickCenter = tick.center * (config.pvMirrorScore ? -1 : 1);

			// keep the shape transform centered at 0, then project as a Sonolus-style billboard
			auto rawPos = Engine::quadvPos(-w, w, noteTop, noteBottom);
			auto vPos = transform.apply(rawPos);
			for (int i = 0; i < 4; ++i)
				applyCameraTiltIconBillboard(vPos[i].x, vPos[i].y, vPos[i].x, vPos[i].y, tickCenter,
				                             y, tickCamera);
			auto uv =
			    Utils::getUV(sprite.getX() / texW, (sprite.getX() + sprite.getWidth()) / texW,
			                 sprite.getY() / texH, (sprite.getY() + sprite.getHeight()) / texH);

			renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint),
			                   (int)texture.getID(),
			                   Engine::getZIndex(SpriteLayer::DIAMOND, tickCenter, y));
		}
	}

	void ScorePreviewWindow::drawHoldCurves(const ScoreContext& context, Renderer* renderer)
	{
		const float total_tm = accumulateDuration(context.scorePreviewDrawData.maxTicks,
		                                          TICKS_PER_BEAT, context.score.tempoChanges);
		const double current_tm =
		    accumulateDuration(context.currentTick, TICKS_PER_BEAT, context.score.tempoChanges);
		const float noteDuration = Engine::getNoteDuration(config.pvNoteSpeed);
		const float mirror = config.pvMirrorScore ? -1 : 1;
		const auto& drawData = context.scorePreviewDrawData;
		const auto layer_stm = getCurrentLayerScaledTimes(context);
		CameraRenderProps pathCamera = getCameraPropsAt(context.score, context.currentTick);
		for (auto& segment : drawData.drawingHoldSegments)
		{
			// 存在チェックを行い、データが既に消えていたら描画を安全にスキップする
			auto endIt = context.score.notes.find(segment.endID);
			if (endIt == context.score.notes.end())
				continue;
			const Note& holdEnd = endIt->second;

			auto startIt = context.score.notes.find(holdEnd.parentID);
			if (startIt == context.score.notes.end())
				continue;
			const Note& holdStart = startIt->second;

			int layer = std::clamp(holdStart.layer, 0, (int)context.score.layers.size() - 1);
			double current_stm = layer_stm[layer];

			if (current_tm >= segment.endTime)
				continue;

			if (std::abs(segment.headTime - segment.tailTime) < 1e-6)
				continue;

			// =======================================================================================
			// ★
			// 完全修正版：STM補間（曲線の維持）と、正確なアンカー固定（途切れ防止）のハイブリッド
			// =======================================================================================

			double start_stm = segment.headTime;
			double start_time = segment.startTime;

			// ノーツが判定ラインを越えた（ホールド中）場合、始点を「現在のSTM」と「現在の時間」に強制固定する
			// これにより、推測計算によるズレが消滅し、絶対に判定ライン（Y=1.0）から帯が途切れません。
			if (current_tm > segment.startTime)
			{
				start_stm = current_stm;
				start_time = current_tm;
			}

			double p_min = 0.0;
			double p_max = 1.0;

			if (p_min >= p_max)
				continue;

			// =======================================================================================

			float holdStartCenter = Engine::getNoteCenter(holdStart) * mirror;
			bool isHoldActivated = current_tm >= segment.activeTime;
			bool isSegmentActivated = current_tm >= segment.startTime;

			int textureID;
			int sprIndex;
			if (segment.isGuide)
			{
				textureID = noteTextures.guideColors;
				auto holdIt = context.score.holdNotes.find(holdStart.ID);
				if (holdIt == context.score.holdNotes.end())
					continue;
				sprIndex = (int)holdIt->second.guideColor;
			}
			else
			{
				textureID = noteTextures.holdPath;
				sprIndex = (!holdStart.critical ? 1 : 3);
			}

			if (textureID == -1)
				continue;
			const Texture& texture = ResourceManager::textures[textureID];
			if (!isArrayIndexInBounds(sprIndex, texture.sprites))
				continue;
			const Sprite& segmentSprite = texture.sprites[sprIndex];

			const auto ease = getEaseFunction(segment.ease);
			float startLeft = segment.headLeft, startRight = segment.headRight,
			      endLeft = segment.tailLeft, endRight = segment.tailRight;

			// 分割数の計算 (元のSTMを用いたロジックを維持)
			double start_y = Engine::approach(start_stm - noteDuration, start_stm, current_stm);
			double end_y =
			    Engine::approach(segment.tailTime - noteDuration, segment.tailTime, current_stm);

			int steps = 10;
			if (segment.ease == EaseType::Linear)
			{
				double mid_travel = (start_y + end_y) / 2.0;
				double perspective_factor = std::pow(std::max(0.1, mid_travel), 0.8);
				double x_diff_max =
				    std::max(std::abs(startLeft - endLeft), std::abs(startRight - endRight));

				// Xの移動量計算には時間割合(time_frac)を使う
				double t_frac_start = unlerpD(segment.startTime, segment.endTime, start_time);
				double t_frac_end = 1.0;
				double x_diff =
				    (x_diff_max * 2.5 / perspective_factor) * std::abs(t_frac_end - t_frac_start);
				double curve_change_scale = std::pow(x_diff, 0.8);
				steps = std::max(1, static_cast<int>(std::ceil(curve_change_scale * 10.0)));
			}
			else
			{
				double pos_offset = 0.0;
				double ref_start_lane =
				    std::abs(startLeft - endLeft) > std::abs(startRight - endRight) ? startLeft
				                                                                    : startRight;
				double ref_end_lane =
				    std::abs(startLeft - endLeft) > std::abs(startRight - endRight) ? endLeft
				                                                                    : endRight;

				double t_frac_start = unlerpD(segment.startTime, segment.endTime, start_time);
				double t_frac_end = 1.0;

				double pos_offset_this_side = 0.0;
				for (double r : { 0.25, 0.75 })
				{
					double time_frac = lerpD(t_frac_start, t_frac_end, r);
					double interp_frac = ease(0.0f, 1.0f, (float)time_frac);
					double y = lerpD(start_y, end_y, r);
					double lane = lerpD(ref_start_lane, ref_end_lane, interp_frac);
					double ref_pos = lerpD(ref_start_lane, ref_end_lane, r);
					double screen_offset = std::abs(lane - ref_pos);
					double compensation_factor = std::pow(std::max(0.1, y), 0.8);
					pos_offset_this_side += screen_offset / compensation_factor;
				}
				pos_offset =
				    pos_offset_this_side * std::pow(std::abs(t_frac_end - t_frac_start), 0.7);
				double curve_change_scale = std::pow(pos_offset, 0.4) * 2.0;
				steps = std::max(1, static_cast<int>(std::ceil(curve_change_scale * 10.0)));
			}
			steps = std::clamp(steps, 1, 200);

			// ==============================================================================

			auto holdMapIt = context.score.holdNotes.find(holdStart.ID);
			if (isSegmentActivated && holdMapIt != context.score.holdNotes.end() &&
			    holdMapIt->second.startType == HoldNoteType::Normal)
			{
				double base_frac = unlerpD(segment.startTime, segment.endTime, start_time);
				float l = ease(startLeft, endLeft, (float)base_frac),
				      r = ease(startRight, endRight, (float)base_frac);
				drawNoteBase(renderer, holdStart, l, r, 1.f, segment.activeTime / total_tm,
				             pathCamera);
				if (holdStart.friction)
					drawTraceDiamond(renderer, holdStart, l, r, 1.f, pathCamera);
			}

			if (config.pvMirrorScore)
			{
				std::swap(startLeft *= -1, startRight *= -1);
				std::swap(endLeft *= -1, endRight *= -1);
			}

			double holdStartProgress, holdEndProgress;
			if (segment.isGuide)
			{
				auto holdMapIt2 = context.score.holdNotes.find(holdStart.ID);
				if (holdMapIt2 == context.score.holdNotes.end())
					continue;
				const HoldNote& hold = holdMapIt2->second;
				double totalJoints = 1 + hold.steps.size();
				double headProgress = segment.tailStepIndex / totalJoints;
				double tailProgress = (segment.tailStepIndex + 1) / totalJoints;

				double base_frac = unlerpD(segment.startTime, segment.endTime, start_time);
				holdStartProgress = lerpD(headProgress, tailProgress, base_frac);
				holdEndProgress = lerpD(headProgress, tailProgress, 1.0);
			}

			double from_percentage = 0;

			// this is the beginning of the hold-head/body X sync fix
			// Y is anchored to "now" via start_stm above, but the lane (X) ease was still being
			// evaluated from time_frac=0 (the segment's original start) every frame, so the body's
			// bottom edge lagged behind the head cap (which correctly uses base_frac). Anchor the
			// X time-fraction the same way so both track the same current position.
			double xBaseFrac = unlerpD(segment.startTime, segment.endTime, start_time);

			// ループ初期値の設定
			double stepStart_stm = lerpD(start_stm, segment.tailTime, p_min);
			double stepTopProgress =
			    unlerpD(stepStart_stm - noteDuration, stepStart_stm, current_stm);
			double stepTop = approachAtTilt((float)stepTopProgress, pathCamera.stageTilt);

			double stepStart_timeFrac = xBaseFrac;

			auto model = DirectX::XMMatrixIdentity();
			float baseAlpha = segment.isGuide ? config.pvGuideAlpha : config.pvHoldAlpha;
			int zIndex = Engine::getZIndex(segment.isGuide ? SpriteLayer::GUIDE_PATH
			                                               : SpriteLayer::HOLD_PATH,
			                               holdStartCenter, segment.activeTime / total_tm);

			for (int i = 0; i < steps; i++)
			{
				double to_p = lerpD(p_min, p_max, (double)(i + 1) / steps);

				// Y座標用には「STM」を補間して使う（純正コードの美しい曲線を維持）
				double stepEnd_stm = lerpD(start_stm, segment.tailTime, to_p);
				double stepBottomProgress =
				    unlerpD(stepEnd_stm - noteDuration, stepEnd_stm, current_stm);
				double stepBottom = approachAtTilt((float)stepBottomProgress, pathCamera.stageTilt);

				// X座標用には「時間割合」を補間して使う（レーンの移動が時間ベースで正確になる）
				double stepEnd_timeFrac = lerpD(xBaseFrac, 1.0, to_p);

				float stepStartLeft = ease(startLeft, endLeft, (float)stepStart_timeFrac);
				float stepEndLeft = ease(startLeft, endLeft, (float)stepEnd_timeFrac);
				float stepStartRight = ease(startRight, endRight, (float)stepStart_timeFrac);
				float stepEndRight = ease(startRight, endRight, (float)stepEnd_timeFrac);

				// =======================================================================================
				// マイナスHS対策：Y座標が逆転した場合、手前と奥の座標を丸ごとスワップしてねじれを防ぐ
				// =======================================================================================
				float q_leftStart = stepStartLeft;
				float q_leftStop = stepEndLeft;
				float q_rightStart = stepStartRight;
				float q_rightStop = stepEndRight;
				float q_top = (float)stepTop;
				float q_bottom = (float)stepBottom;

				if (q_top < q_bottom)
				{
					std::swap(q_top, q_bottom);
					std::swap(q_leftStart, q_leftStop);
					std::swap(q_rightStart, q_rightStop);
				}

				auto vPos = Engine::perspectiveQuadvPos(q_leftStart, q_leftStop, q_rightStart,
				                                        q_rightStop, q_top, q_bottom);
				for (auto& v : vPos)
					applyCameraTilt(v.x, v.y, pathCamera);
				// =======================================================================================

				float spr_x1, spr_x2, spr_y1, spr_y2;
				std::array<DirectX::XMFLOAT4, 4> vertexColors;

				if (segment.isGuide)
				{
					auto holdMapIt3 = context.score.holdNotes.find(holdStart.ID);
					if (holdMapIt3 == context.score.holdNotes.end())
						continue;
					const HoldNote& hold = holdMapIt3->second;
					double startProg = lerpD(holdStartProgress, holdEndProgress, from_percentage);
					double to_percentage = double(i + 1) / steps;
					double endProg = lerpD(holdStartProgress, holdEndProgress, to_percentage);
					float startAlpha = baseAlpha;
					float endAlpha = baseAlpha;
					if (hold.fadeType == FadeType::Out)
					{
						startAlpha *= (1.0f - (float)startProg);
						endAlpha *= (1.0f - (float)endProg);
					}
					else if (hold.fadeType == FadeType::In)
					{
						startAlpha *= (float)startProg;
						endAlpha *= (float)endProg;
					}
					vertexColors = {
						{ toFloat4(defaultTint, startAlpha), toFloat4(defaultTint, endAlpha),
						  toFloat4(defaultTint, endAlpha), toFloat4(defaultTint, startAlpha) }
					};
					spr_x1 = segmentSprite.getX();
					spr_x2 = segmentSprite.getX() + segmentSprite.getWidth();
					spr_y1 = segmentSprite.getY() + segmentSprite.getHeight();
					spr_y2 = segmentSprite.getY();
					from_percentage = to_percentage;
				}
				else
				{
					spr_x1 = segmentSprite.getX() + HOLD_XCUTOFF;
					spr_x2 = segmentSprite.getX() + segmentSprite.getWidth() - HOLD_XCUTOFF;
					spr_y1 = segmentSprite.getY();
					spr_y2 = segmentSprite.getY() + segmentSprite.getHeight();
				}

				float texW = (float)texture.getWidth();
				float texH = (float)texture.getHeight();
				auto uv = Utils::getUV(spr_x1 / texW, spr_x2 / texW, spr_y1 / texH, spr_y2 / texH);

				if (config.pvHoldAnimation && isHoldActivated && !segment.isGuide &&
				    isArrayIndexInBounds(sprIndex - 1, texture.sprites))
				{
					const Sprite& activeSprite = texture.sprites[sprIndex - 1];
					const int norm2ActiveOffset = (int)(activeSprite.getY() - segmentSprite.getY());
					double delta_tm = current_tm - segment.activeTime;
					float normalAplha = (std::cos((float)delta_tm * MATH_PI * 2.f) + 2.f) / 3.f;
					renderer->pushQuad(vPos, uv, model,
					                   toFloat4(defaultTint, baseAlpha * normalAplha),
					                   (int)texture.getID(), zIndex);
					auto uvActive = Utils::getUV(spr_x1 / texW, spr_x2 / texW,
					                             (spr_y1 + norm2ActiveOffset) / texH,
					                             (spr_y2 + norm2ActiveOffset) / texH);
					renderer->pushQuad(vPos, uvActive, model,
					                   toFloat4(defaultTint, baseAlpha * (1.f - normalAplha)),
					                   (int)texture.getID(), zIndex);
				}
				else if (segment.isGuide)
				{
					renderer->pushQuad(vPos, uv, model, vertexColors, (int)texture.getID(), zIndex);
				}
				else
				{
					renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, baseAlpha),
					                   (int)texture.getID(), zIndex);
				}

				// ループ終端の更新処理
				stepTop = stepBottom;
				stepStart_timeFrac = stepEnd_timeFrac;
			}
		}
	}

	void ScorePreviewWindow::drawNoteBase(Renderer* renderer, const Note& note, float noteLeft,
	                                      float noteRight, float y, float zScalar,
	                                      const CameraRenderProps& camera)
	{
		int textureID =
		    note.getType() == NoteType::Damage ? noteTextures.ccNotes : noteTextures.notes;
		if (textureID == -1)
			return;
		const Texture& texture = ResourceManager::textures[textureID];

		const int sprIndex = note.getType() == NoteType::Damage ? getCcNoteSpriteIndex(note)
		                                                        : getNoteSpriteIndex(note);
		if (!isArrayIndexInBounds(sprIndex, texture.sprites))
			return;
		const Sprite& sprite = texture.sprites[sprIndex];

		size_t transIndexM = static_cast<size_t>(SpriteType::NoteMiddle);
		size_t transIndexL = static_cast<size_t>(SpriteType::NoteLeft);
		size_t transIndexR = static_cast<size_t>(SpriteType::NoteRight);
		if (!isArrayIndexInBounds(transIndexM, ResourceManager::spriteTransforms) ||
		    !isArrayIndexInBounds(transIndexL, ResourceManager::spriteTransforms) ||
		    !isArrayIndexInBounds(transIndexR, ResourceManager::spriteTransforms))
			return;

		const SpriteTransform& mTransform = ResourceManager::spriteTransforms[transIndexM];
		const SpriteTransform& lTransform = ResourceManager::spriteTransforms[transIndexL];
		const SpriteTransform& rTransform = ResourceManager::spriteTransforms[transIndexR];

		const float noteHeight = Engine::getNoteHeight();
		const float noteTop = 1.f - noteHeight;
		const float noteBottom = 1.f + noteHeight;
		if (config.pvMirrorScore)
			std::swap(noteLeft *= -1.f, noteRight *= -1.f);
		int zIndex =
		    Engine::getZIndex(!note.friction ? SpriteLayer::BASE_NOTE : SpriteLayer::TICK_NOTE,
		                      noteLeft + (noteRight - noteLeft) / 2.f, y * zScalar);

		auto applyCamera = [&](std::array<DirectX::XMFLOAT4, 4>& vp)
		{
			for (auto& v : vp)
				applyCameraTiltIcon(v.x, v.y, v.y, (float)y, camera);
		};

		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();

		std::array<DirectX::XMFLOAT4, 4> vPos, uv;

		// ---------------------------------------------------------
		// 中央パーツのぼやけ（エイリアシング）回避ロジック
		// ---------------------------------------------------------
		float middleLeft = noteLeft + 0.25f;
		float middleRight = noteRight - 0.3f;
		float geomWidth = middleRight - middleLeft;

		float midUvLeft = sprite.getX() + NOTE_SIDE_WIDTH;
		float midUvRight = sprite.getX() + sprite.getWidth() - NOTE_SIDE_WIDTH;

		// 描画幅が狭い場合、テクスチャが圧縮されてぼやけるのを防ぐため、
		// UV領域もジオメトリ幅に合わせて中央部分のみをクロップ（切り出し）する
		if (geomWidth > 0.0f)
		{
			float maxUvWidth = geomWidth * 100.0f; // 1ユニットあたりの適正テクスチャピクセル数
			float currentUvWidth = midUvRight - midUvLeft;

			if (currentUvWidth > maxUvWidth)
			{
				float centerUv = (midUvLeft + midUvRight) / 2.0f;
				midUvLeft = centerUv - (maxUvWidth / 2.0f);
				midUvRight = centerUv + (maxUvWidth / 2.0f);
			}
		}

		auto makeQuad = [&](float l, float r) -> std::array<DirectX::XMFLOAT4, 4>
		{ return Engine::perspectiveQuadvPos(l, r, noteTop, noteBottom); };

		if (geomWidth > 0.0f)
		{
			vPos = mTransform.apply(makeQuad(middleLeft, middleRight));
			applyCamera(vPos);
			uv = Utils::getUV(midUvLeft / texW, midUvRight / texW, sprite.getY() / texH,
			                  (sprite.getY() + sprite.getHeight()) / texH);
			renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint),
			                   (int)texture.getID(), zIndex);
		}

		// Left slice (純正完全維持)
		vPos = lTransform.apply(makeQuad(noteLeft, noteLeft + 0.25f));
		applyCamera(vPos);
		uv = Utils::getUV((sprite.getX() + NOTE_SIDE_PAD) / texW,
		                  (sprite.getX() + NOTE_SIDE_WIDTH) / texW, sprite.getY() / texH,
		                  (sprite.getY() + sprite.getHeight()) / texH);
		renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint),
		                   (int)texture.getID(), zIndex);

		// Right slice (純正完全維持)
		vPos = rTransform.apply(makeQuad(noteRight - 0.3f, noteRight));
		applyCamera(vPos);
		uv = Utils::getUV((sprite.getX() + sprite.getWidth() - NOTE_SIDE_WIDTH) / texW,
		                  (sprite.getX() + sprite.getWidth() - NOTE_SIDE_PAD) / texW,
		                  sprite.getY() / texH, (sprite.getY() + sprite.getHeight()) / texH);
		renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint),
		                   (int)texture.getID(), zIndex);
	}

	void ScorePreviewWindow::drawTraceDiamond(Renderer* renderer, const Note& note, float noteLeft,
	                                          float noteRight, float y,
	                                          const CameraRenderProps& camera)
	{

		if (noteTextures.notes == -1)
			return;
		const Texture& texture = getNoteTexture();
		int frictionSprIndex = getFrictionSpriteIndex(note);
		if (!isArrayIndexInBounds(frictionSprIndex, texture.sprites))
			return;
		const Sprite& frictionSpr = texture.sprites[frictionSprIndex];

		size_t transIndex = static_cast<size_t>(SpriteType::TraceDiamond);
		if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms))
			return;
		const SpriteTransform& transform = ResourceManager::spriteTransforms[transIndex];

		const float w = Engine::getNoteHeight() / scaledAspectRatio;
		const float noteTop = 1.f + Engine::getNoteHeight(),
		            noteBottom = 1.f - Engine::getNoteHeight();
		if (config.pvMirrorScore)
			std::swap(noteLeft *= -1.f, noteRight *= -1.f);
		const float noteCenter = noteLeft + (noteRight - noteLeft) / 2.f;
		int zIndex = Engine::getZIndex(SpriteLayer::DIAMOND, noteCenter, y);

		// keep the shape transform centered at 0, then project as a Sonolus-style billboard
		auto rawPos = Engine::quadvPos(-w, w, noteTop, noteBottom);
		auto vPos = transform.apply(rawPos);
		for (int i = 0; i < 4; ++i)
			applyCameraTiltIconBillboard(vPos[i].x, vPos[i].y, vPos[i].x, vPos[i].y, noteCenter,
			                             (float)y, camera);

		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();
		auto uv = Utils::getUV(
		    frictionSpr.getX() / texW, (frictionSpr.getX() + frictionSpr.getWidth()) / texW,
		    frictionSpr.getY() / texH, (frictionSpr.getY() + frictionSpr.getHeight()) / texH);

		renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint),
		                   (int)texture.getID(), zIndex);
	}

	void ScorePreviewWindow::drawFlickArrow(Renderer* renderer, const Note& note, float y,
	                                        double time, const CameraRenderProps& camera)
	{
		if (noteTextures.notes == -1)
			return;
		const Texture& texture = getNoteTexture();
		const int sprIndex = getFlickArrowSpriteIndex(note);
		if (!isArrayIndexInBounds(sprIndex, texture.sprites))
			return;
		const Sprite& arrowSprite = texture.sprites[sprIndex];

		//  DownLeft, DownRight も左右フリックとして扱うように判定を追加
		bool isLeftOrRight =
		    (note.flick == FlickType::Left || note.flick == FlickType::Right ||
		     note.flick == FlickType::DownLeft || note.flick == FlickType::DownRight);
		bool isRightward = (note.flick == FlickType::Right || note.flick == FlickType::DownRight);

		size_t flickTransformIdx =
		    std::clamp((int)note.width, 1, MAX_FLICK_SPRITES) - 1 +
		    static_cast<int>(isLeftOrRight ? SpriteType::FlickArrowLeft : SpriteType::FlickArrowUp);
		if (!isArrayIndexInBounds(flickTransformIdx, ResourceManager::spriteTransforms))
			return;
		const SpriteTransform& transform = ResourceManager::spriteTransforms[flickTransformIdx];

		const int mirror = config.pvMirrorScore ? -1 : 1;
		const int flickDirection = mirror * (isLeftOrRight ? (isRightward ? 1 : -1) : 0);
		const float center = Engine::getNoteCenter(note) * mirror;
		const float w = std::clamp((int)note.width, 1, MAX_FLICK_SPRITES) *
		                (isRightward ? -1.f : 1.f) * mirror / 4.f;

		// this is the beginning of the lane-offset fix: keep the icon shape transform centered
		// at 0 (instead of feeding it the note's absolute lane position) so it isn't sheared
		// for notes far from lane 0, then project as a Sonolus-style billboard (single-depth
		// anchor + screen-space width/perp vectors, matching layout_flick_arrow)
		auto rawPos = Engine::quadvPos(-w, w, 1.f, 1.f - 2.f * std::abs(w) * scaledAspectRatio);
		auto vPos = transform.apply(rawPos);
		for (int i = 0; i < 4; ++i)
			applyCameraTiltIconBillboard(vPos[i].x, vPos[i].y, vPos[i].x, vPos[i].y, center,
			                             (float)y, camera);

		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();
		auto uv = Utils::getUV(
		    arrowSprite.getX() / texW, (arrowSprite.getX() + arrowSprite.getWidth()) / texW,
		    arrowSprite.getY() / texH, (arrowSprite.getY() + arrowSprite.getHeight()) / texH);

		//  下フリックの場合、UV座標のY軸を入れ替えて画像を上下反転させる
		bool isDown = (note.flick >= FlickType::Down && note.flick <= FlickType::DownRight);
		if (isDown)
		{
			// Utils::getUV は {右上, 右下, 左下, 左上}
			// の順なので、0と1(右側)、3と2(左側)のY座標を入れ替える
			std::swap(uv[0].y, uv[1].y);
			std::swap(uv[3].y, uv[2].y);
		}

		int zIndex = Engine::getZIndex(SpriteLayer::FLICK_ARROW, center, y);

		if (config.pvFlickAnimation)
		{
			double t = std::fmod(time, 0.5) / 0.5;
			auto cubicEaseIn = [](double val) { return (float)(val * val * val); };
			// bob distance shrinks with the note's depth (tilt_width_factor(travel)) so it stays
			// proportional to the icon's own size instead of a fixed screen-space offset
			float depthScale = cameraTiltedWidth((float)y, camera.stageTilt);
			auto animationVector = DirectX::XMVectorScale(
			    DirectX::XMVectorSet((float)flickDirection, -2.f * scaledAspectRatio, 0.f, 0.f),
			    (float)t * depthScale);
			auto model = DirectX::XMMatrixTranslationFromVector(animationVector);
			renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, 1.f - cubicEaseIn(t)),
			                   (int)texture.getID(), zIndex);
		}
		else
		{
			renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint),
			                   (int)texture.getID(), zIndex);
		}
	}

	void ScorePreviewWindow::updateToolbar(ScoreEditorTimeline& timeline,
	                                       ScoreContext& context) const
	{
		static float lastHoveredTime = -1;
		constexpr float MAX_NO_HOVER_TIME = 1.5f;
		static float toolBarWidth = UI::btnNormal.x * 2;
		if (!config.pvDrawToolbar)
			return;
		ImGuiIO io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImGui::GetWindowPos() +
		                        ImVec2{ ImGui::GetContentRegionAvail().x -
		                                    ImGui::GetStyle().WindowPadding.x * 4 - toolBarWidth,
		                                ImGui::GetStyle().WindowPadding.y * 5 });
		ImGui::SetNextWindowSizeConstraints({ 48, 0 }, { 120, FLT_MAX }, NULL);
		auto easeInCubic = [](float t) { return t * t * t; };
		float childBgAlpha =
		    std::clamp(easeInCubic(unlerp(MAX_NO_HOVER_TIME, 0.f, lastHoveredTime)), 0.25f, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg,
		                      ImGui::GetColorU32(ImGuiCol_WindowBg, childBgAlpha));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

		ImGui::Begin("###preview_toolbar", NULL,
		             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
		                 ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar |
		                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
		                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_ChildWindow);
		toolBarWidth = ImGui::GetWindowWidth();
		float centeredXBtn = toolBarWidth / 2 - UI::btnNormal.x / 2;
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			lastHoveredTime = 0;
		else
			lastHoveredTime = std::min(io.DeltaTime + lastHoveredTime, MAX_NO_HOVER_TIME);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_DOUBLE_UP, UI::btnNormal, true,
		                          context.currentTick <
		                              context.scorePreviewDrawData.maxTicks + TICKS_PER_BEAT))
		{
			if (timeline.isPlaying())
				timeline.setPlaying(context, false);
			context.currentTick =
			    timeline.roundTickDown(context.currentTick, timeline.getDivision()) +
			    (TICKS_PER_BEAT / (timeline.getDivision() / 4));
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_UP, UI::btnNormal, true,
		                          context.currentTick <
		                              context.scorePreviewDrawData.maxTicks + TICKS_PER_BEAT))
		{
			if (timeline.isPlaying())
				timeline.setPlaying(context, false);
			context.currentTick++;
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_STOP, UI::btnNormal, false))
			timeline.stop(context);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(timeline.isPlaying() ? ICON_FA_PAUSE : ICON_FA_PLAY,
		                          UI::btnNormal))
			timeline.setPlaying(context, !timeline.isPlaying());

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_DOWN, UI::btnNormal, true, context.currentTick > 0))
		{
			if (timeline.isPlaying())
				timeline.setPlaying(context, false);
			context.currentTick--;
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_DOUBLE_DOWN, UI::btnNormal, true,
		                          context.currentTick > 0))
		{
			if (timeline.isPlaying())
				timeline.setPlaying(context, false);
			context.currentTick =
			    std::max(timeline.roundTickDown(context.currentTick, timeline.getDivision()) -
			                 (TICKS_PER_BEAT / (timeline.getDivision() / 4)),
			             0);
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(isFullWindow() ? ICON_FA_COMPRESS : ICON_FA_EXPAND))
			fullWindow = !isFullWindow();

		ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_MINUS, UI::btnNormal, false,
		                          timeline.getPlaybackSpeed() > 0.25f))
			timeline.setPlaybackSpeed(context, timeline.getPlaybackSpeed() - 0.25f);

		const float playbackStrWidth = ImGui::CalcTextSize("0000%").x;
		ImGui::SetCursorPosX(toolBarWidth / 2 - playbackStrWidth / 2);
		UI::transparentButton(IO::formatString("%.0f%%", timeline.getPlaybackSpeed() * 100).c_str(),
		                      ImVec2{ playbackStrWidth, UI::btnNormal.y }, false, false);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_PLUS, UI::btnNormal, false,
		                          timeline.getPlaybackSpeed() < 1.0f))
			timeline.setPlaybackSpeed(context, timeline.getPlaybackSpeed() + 0.25f);

		ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

		float currentTm =
		    accumulateDuration(context.currentTick, TICKS_PER_BEAT, context.score.tempoChanges);

		//  ツールバーの表示用には、現在選択されているレイヤーの視覚的時間を用いる
		int currentLayer =
		    std::clamp(context.selectedLayer, 0, (int)context.score.layers.size() - 1);
		double currentScaledTm =
		    getCachedLayerScaledTime(context, context.currentTick, currentLayer);
		int currentMeasure =
		    accumulateMeasures(context.currentTick, TICKS_PER_BEAT, context.score.timeSignatures);
		const TimeSignature& ts =
		    context.score
		        .timeSignatures[findTimeSignature(currentMeasure, context.score.timeSignatures)];
		const Tempo& tempo = getTempoAt(context.currentTick, context.score.tempoChanges);
		int hiSpeedIdx = findHighSpeedChange(context.currentTick, context.score.hiSpeedChanges, 0);
		float speed = (hiSpeedIdx == -1 ? 1.0f : context.score.hiSpeedChanges[hiSpeedIdx].speed);

		char rhythmString[256];
		snprintf(rhythmString, sizeof(rhythmString), "%02d:%02d:%02d|%.2fs|%d/%d|%g BPM|%.2fx",
		         static_cast<int>(currentTm / 60), static_cast<int>(std::fmod(currentTm, 60.f)),
		         static_cast<int>(std::fmod(currentTm * 100, 100.f)), currentScaledTm, ts.numerator,
		         ts.denominator, tempo.bpm, speed);
		char* str = strtok(rhythmString, "|");
		ImGui::SetCursorPosX(toolBarWidth / 2 - ImGui::CalcTextSize(str).x / 2);
		ImGui::Text(str);
		for (auto&& col : { feverColor, timeColor, tempoColor, speedColor })
		{
			str = strtok(NULL, "|");
			ImGui::SetCursorPosX(toolBarWidth / 2 - ImGui::CalcTextSize(str).x / 2);
			ImGui::TextColored(ImColor(col), str);
		}
		ImGui::EndChild();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}

	float ScorePreviewWindow::getScrollbarWidth() const
	{
		return ImGui::GetStyle().ScrollbarSize + 4.f;
	}

	void ScorePreviewWindow::updateScrollbar(ScoreEditorTimeline& timeline,
	                                         ScoreContext& context) const
	{
		constexpr float scrollpadY = 30.f;
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 contentSize = ImGui::GetWindowContentRegionMax();
		ImVec2 cursorBegPos = ImGui::GetCursorStartPos();
		ImVec2 scrollbarSize = { getScrollbarWidth(), contentSize.y - cursorBegPos.y };

		ImGui::SetCursorPos(
		    cursorBegPos +
		    ImVec2{ contentSize.x - scrollbarSize.x - style.WindowPadding.x / 2, 0 });
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarBg));
		ImGui::BeginChild("###scrollbar", scrollbarSize, false,
		                  ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleColor();

		ImVec2 scrollContentSize = ImGui::GetContentRegionAvail();
		ImVec2 scrollMaxSize = ImGui::GetWindowContentRegionMax();
		int maxTicks = std::max(context.scorePreviewDrawData.maxTicks, 1);

		float scrollRatio =
		    std::min((float)(Engine::getNoteDuration(config.pvNoteSpeed) /
		                     accumulateDuration(context.scorePreviewDrawData.maxTicks,
		                                        TICKS_PER_BEAT, context.score.tempoChanges)),
		             1.f);

		float progress = 1.f - std::min(float(context.currentTick) / maxTicks, 1.f);
		float handleHeight = std::max(20.f, scrollContentSize.y * scrollRatio);

		bool scrollbarActive = false;
		ImGui::BeginDisabled(timeline.isPlaying());
		ImGui::SetCursorPos(ImGui::GetCursorStartPos());
		ImGui::InvisibleButton("##scroll_bg", contentSize, ImGuiButtonFlags_NoNavFocus);
		scrollbarActive |= ImGui::IsItemActive();

		ImVec2 handleSize = { style.ScrollbarSize, handleHeight };
		ImVec2 handlePos = { scrollMaxSize.x / 2 - handleSize.x / 2,
			                 lerp(0.f, scrollMaxSize.y - handleHeight, progress) };
		ImVec2 absHandlePos = ImGui::GetWindowPos() + handlePos;

		ImGui::SetCursorPos(handlePos);
		ImGui::InvisibleButton("##scroll_handle", handleSize);
		scrollbarActive |= ImGui::IsItemActive();

		ImGuiCol_ handleColBase = scrollbarActive          ? ImGuiCol_ScrollbarGrabActive
		                          : ImGui::IsItemHovered() ? ImGuiCol_ScrollbarGrabHovered
		                                                   : ImGuiCol_ScrollbarGrab;

		ImGui::RenderFrame(absHandlePos, absHandlePos + ImGui::GetItemRectSize(),
		                   ImGui::GetColorU32(handleColBase), true, 3.f);
		ImGui::EndDisabled();

		if (scrollbarActive)
		{
			float absScrollStart = ImGui::GetWindowPos().y + handleSize.y / 2.f;
			float absScrollEnd = ImGui::GetWindowPos().y + scrollMaxSize.y - handleSize.y / 2.f;
			float mouseProgress =
			    1.f - std::clamp(unlerp(absScrollStart, absScrollEnd, io.MousePos.y), 0.f, 1.f);
			context.currentTick = (int)std::round(lerp(0.f, (float)maxTicks, mouseProgress));
		}
		ImGui::EndChild();
	}
}