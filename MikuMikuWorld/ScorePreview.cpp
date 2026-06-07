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

	inline DirectX::XMFLOAT4 toFloat4(const Color& c, float alphaScale = 1.0f) {
		return { c.r, c.g, c.b, c.a * alphaScale };
	}

	namespace Utils
	{
		inline void fitRect(float target_width, float target_height, long double source_aspect_ratio, float& width, float& height)
		{
			const float target_aspect_ratio = target_width / target_height;
			width  = target_aspect_ratio > source_aspect_ratio ? (float)source_aspect_ratio * target_height : target_width;
			height = target_aspect_ratio < source_aspect_ratio ? target_width / (float)source_aspect_ratio : target_height;
		}

		inline void fillRect(float target_width, float target_height, long double source_aspect_ratio, float& width, float& height)
		{
			const float target_aspect_ratio = target_width / target_height;
			width  = target_aspect_ratio < source_aspect_ratio ? (float)source_aspect_ratio * target_height : target_width;
			height = target_aspect_ratio > source_aspect_ratio ? target_width / (float)source_aspect_ratio : target_height;
		}

		inline std::array<DirectX::XMFLOAT4, 4> getUV(float left, float right, float top, float bottom)
		{
			return {{
				{ right, top,    0.f, 1.f }, 
				{ right, bottom, 0.f, 1.f }, 
				{ left,  bottom, 0.f, 1.f }, 
				{ left,  top,    0.f, 1.f }  
			}};
		}
	};

	static const int NOTE_SIDE_WIDTH = 91;
	static const int NOTE_SIDE_PAD = 10;
	static const int MAX_FLICK_SPRITES = 6;
	static const int HOLD_XCUTOFF = 36;
	static const int GUIDE_XCUTOFF = 3;
	static const int GUIDE_Y_TOP_CUTOFF = -41;
	static const int GUIDE_Y_BOTTOM_CUTOFF = -12;
	static Color defaultTint { 1.f, 1.f, 1.f, 1.f };

	static double getCachedLayerScaledTime(const ScoreContext& context, int tick, int layer)
	{
		if (layer >= 0
			&& layer < static_cast<int>(context.scorePreviewDrawData.hsCache.size())
			&& !context.scorePreviewDrawData.hsCache[layer].nodes.empty())
		{
			return context.scorePreviewDrawData.hsCache[layer].getStm(tick);
		}

		return Engine::accumulateScaledDuration(tick, TICKS_PER_BEAT, context.score.tempoChanges, context.score.hiSpeedChanges, layer);
	}

	static std::vector<double> getCurrentLayerScaledTimes(const ScoreContext& context)
	{
		std::vector<double> layerStm(context.score.layers.size());
		for (int i = 0; i < static_cast<int>(context.score.layers.size()); ++i)
			layerStm[i] = getCachedLayerScaledTime(context, context.currentTick, i);
		return layerStm;
	}

	ScorePreviewBackground::ScorePreviewBackground() : backgroundFile(), jacketFile{}, brightness(0.5f), frameBuffer{2048, 2048}, init{false} {}

	ScorePreviewBackground::~ScorePreviewBackground()
	{
		frameBuffer.dispose();
	}

	void ScorePreviewBackground::setBrightness(float value)
	{
		brightness = value;
	}

	void ScorePreviewBackground::update(Renderer* renderer, const Jacket& jacket)
	{
		init = true;
		backgroundFile = config.backgroundImage;
		jacketFile = jacket.getFilename();
		brightness = config.pvBackgroundBrightness;
		bool useDefaultTexture = backgroundFile.empty() || !IO::File::exists(backgroundFile);
		
		Texture backgroundTex = { useDefaultTexture ? Application::getAppDir() + "res\\textures\\default.png" : backgroundFile};
		const float bgWidth = backgroundTex.getWidth(), bgHeight = backgroundTex.getHeight();
		if (bgWidth != frameBuffer.getWidth() || bgHeight != frameBuffer.getHeight())
			frameBuffer.resize((unsigned int)bgWidth, (unsigned int)bgHeight);
		frameBuffer.bind();
		frameBuffer.clear();
		
		int shaderId;
		if ((shaderId = ResourceManager::getShader("basic2d")) == -1) return;
		Shader* basicShader = ResourceManager::shaders[shaderId];
		
		basicShader->use();

		const float projectionX{ std::max(bgWidth, 10.f) };
		const float projectionY{ std::max(bgHeight, 10.f) };
		basicShader->setMatrix4("projection", Camera().getOffCenterOrthographicProjection(0, projectionX, 0, projectionY)); 
		
		if (backgroundTex.getID() > 0)
		{
			renderer->beginBatch();
			renderer->drawRectangle(Vector2(0, 0), Vector2(bgWidth, bgHeight), backgroundTex, 0, bgWidth, 0, bgHeight, defaultTint, 0);
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
		maskShader->setMatrix4("projection", Camera().getOffCenterOrthographicProjection(0.f, 2048.f, 0.f, 2048.f));
		renderer->pushQuadMasked(mainLeftPos, DefaultJacket::getLeftUV(), mainLeftMask, mainCol, jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mainRightPos, DefaultJacket::getRightUV(), mainRightMask, mainCol, jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mirrorLeftPos, DefaultJacket::getLeftMirrorUV(), mirrorLeftMask, mirrorCol, jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mirrorRightPos, DefaultJacket::getRightMirrorUV(), mirrorRightMask, mirrorCol, jacket.getTexID(), stage.getID());

		basicShader->use();
		basicShader->setMatrix4("projection", Camera().getOffCenterOrthographicProjection(0.f, 2048.f, 0.f, 2048.f));
		renderer->beginBatch();
		renderer->pushQuad(mainLeftPos, mainLeftMask, DirectX::XMMatrixIdentity(), defCol, stage.getID(), 0);
		renderer->pushQuad(mainRightPos, mainRightMask, DirectX::XMMatrixIdentity(), defCol, stage.getID(), 0);
		renderer->pushQuad(mirrorLeftPos, mirrorLeftMask, DirectX::XMMatrixIdentity(), defCol, stage.getID(), 0);
		renderer->pushQuad(mirrorRightPos, mirrorRightMask, DirectX::XMMatrixIdentity(), defCol, stage.getID(), 0);

		const Sprite* sprite = &stage.sprites[SPR_JACKET_WINDOW];
		renderer->drawRectangle(Vector2(682.f, 497.f), Vector2(686.f, 686.f), stage,
		                        sprite->getX(), sprite->getX() + sprite->getWidth(),
		                        sprite->getY(), sprite->getY() + sprite->getHeight(), defaultTint, 1);

		sprite = &stage.sprites[SPR_MIRROR_JACKET_WINDOW];
		renderer->drawRectangle(Vector2(699.f, 958.f), Vector2(651.f, 650.f), stage,
		                        sprite->getX(), sprite->getX() + sprite->getWidth(),
		                        sprite->getY(), sprite->getY() + sprite->getHeight(),
		                        Color(defaultTint.r, defaultTint.g, defaultTint.b, defaultTint.a * 0.6f), 1);
		renderer->endBatch();

		auto mainWindowPos = Engine::quadvPos(824.f, 824.f + 400.f, 666.f, 666.f + 384.f);
		auto mainWindowMask = Engine::quadUV(stage.sprites[SPR_JACKET_MASK], stage);
		auto mirrorWindowPos = Engine::quadvPos(834.f, 834.f + 386.f, 1120.f, 1120.f + 336.f);
		auto mirrorWindowMask = Engine::quadUV(stage.sprites[SPR_MIRROR_JACKET_MASK], stage);

		maskShader->use();
		renderer->pushQuadMasked(mainWindowPos, DefaultJacket::getCenterUV(), mainWindowMask,
		                         { 1.f, 1.f, 1.f, 0.8f }, jacket.getTexID(), stage.getID());
		renderer->pushQuadMasked(mirrorWindowPos, DefaultJacket::getMirrorCenterUV(), mirrorWindowMask,
		                         { 1.f, 1.f, 1.f, 0.5f }, jacket.getTexID(), stage.getID());

		basicShader->use();
		renderer->beginBatch();
		renderer->pushQuad(mainWindowPos, mainWindowMask, DirectX::XMMatrixIdentity(), defCol, stage.getID(), 0);
		renderer->pushQuad(mirrorWindowPos, mirrorWindowMask, DirectX::XMMatrixIdentity(), defCol, stage.getID(), 0);

		sprite = &stage.sprites[SPR_SEKAI_FLOOR];
		renderer->drawRectangle(Vector2(0.f, 1251.f), Vector2(sprite->getWidth(), sprite->getHeight()), stage,
		                        sprite->getX(), sprite->getX() + sprite->getWidth(),
		                        sprite->getY(), sprite->getY() + sprite->getHeight(),
		                        Color(defaultTint.r, defaultTint.g, defaultTint.b, defaultTint.a * 0.8f), 1);
		renderer->endBatch();
	}

	bool ScorePreviewBackground::shouldUpdate(const Jacket& jacket) const
	{
		return !init || backgroundFile != config.backgroundImage || jacketFile != jacket.getFilename();
	}

	void ScorePreviewBackground::draw(Renderer *renderer, float scrWidth, float scrHeight) const
	{
		float bgScrWidth = (float)frameBuffer.getWidth(), bgScrHeight = (float)frameBuffer.getHeight(), targetWidth, targetHeight;
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
		const float centerY = 0.5f / Engine::STAGE_HEIGHT_RATIO + Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const float bgTop = centerY + -bgHeight / 2.f;
		auto vPos = Engine::quadvPos(bgLeft, bgLeft + bgWidth, bgTop, bgTop + bgHeight);
		auto uv = Utils::getUV(0.f, 1.f, 0.f, 1.f);
		renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), DirectX::XMFLOAT4(brightness, brightness, brightness, 1.f), frameBuffer.getTexture(), -10);
	}

	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getLeftUV() { return {{ { 303.8f / 740, 504.8f / 740, 0, 0 }, { 317.5f / 740, 297.7f / 740, 0, 0 }, { 5.5f / 740, 278.3f / 740, 0, 0 }, { -8.f / 740, 497.4f / 740, 0, 0 } }}; }
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getRightUV() { return {{ { 749.5f / 740, 377.7f / 740, 0, 0 }, { 738.2f / 740, 188.1f / 740, 0, 0 }, { 415.0f / 740, 171.4f / 740, 0, 0 }, { 432.1f / 740, 363.9f / 740, 0, 0 } }}; }
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getLeftMirrorUV() { return {{ { 292.761414f / 740, 247.401382f / 740, 0, 0 }, { 310.765869f / 740, 491.944763f / 740, 0, 0 }, { 6.892246f / 740, 498.470642f / 740, 0, 0 }, { -6.246704f / 740, 258.264862f / 740, 0, 0 } }}; }
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getRightMirrorUV() { return {{ { 733.444458f / 740, 183.954681f / 740, 0, 0 }, { 743.541321f / 740, 355.960449f / 740, 0, 0 }, { 418.899414f / 740, 332.759491f / 740, 0, 0 }, { 410.746246f / 740, 155.907684f / 740, 0, 0 } }}; }
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getCenterUV() { return {{ { 755.541687f / 740, 744.057861f / 740, 0, 0 }, { 739.961182f / 740, -1.859504f / 740, 0, 0 }, { 0.043696f / 740, -1.859504f / 740, 0, 0 }, { -17.484388f / 740, 744.057861f / 740, 0, 0 } }}; }
	std::array<DirectX::XMFLOAT4, 4> ScorePreviewBackground::DefaultJacket::getMirrorCenterUV() { return {{ { 747.697083f / 740, 2.164453f / 740, 0, 0 }, { 743.909424f / 740, 731.297241f / 740, 0, 0 }, { -1.864066f / 740, 731.297241f / 740, 0, 0 }, { 3.837242f / 740, 2.164453f / 740, 0, 0 } }}; }

	ScorePreviewWindow::ScorePreviewWindow() : previewBuffer{ 1920, 1080 }, background(), scaledAspectRatio(1)
	{
		noteEffectsCamera.setFov(50.f);
		noteEffectsCamera.setRotation(-90.f, 27.1f);
		noteEffectsCamera.setPosition({ 0, 5.32f, -5.86f, 0 });
		noteEffectsCamera.positionCamNormal();
	}

	ScorePreviewWindow::~ScorePreviewWindow() {}

	void ScorePreviewWindow::update(ScoreContext& context, Renderer* renderer)
	{
		bool isWindowActive =  !ImGui::IsWindowDocked() || ImGui::GetCurrentWindow()->TabId == ImGui::GetWindowDockNode()->SelectedTabId;
		if (!isWindowActive) return;

		bool needsRecalc = false;

		if (context.scorePreviewDrawData.noteSpeed != config.pvNoteSpeed)
			needsRecalc = true;

		if (!needsRecalc && !context.score.notes.empty() && context.scorePreviewDrawData.drawingNotes.empty())
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
				if (++checkCount > 5) break; 
			}
		}

		if (needsRecalc)
		{
			context.scorePreviewDrawData.calculateDrawData(context.score);
		}

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
		if (shaderId == -1 || pteShaderId == -1) return;

		Shader* shader = ResourceManager::shaders[shaderId];
		Shader* pteShader = ResourceManager::shaders[pteShaderId];
		shader->use();

		float width  = size.x, height = size.y;
		float scaledWidth = Engine::STAGE_TARGET_WIDTH * Engine::STAGE_WIDTH_RATIO;
		float scaledHeight = Engine::STAGE_TARGET_HEIGHT * Engine::STAGE_HEIGHT_RATIO;
		float scrTop  = Engine::STAGE_TARGET_HEIGHT * Engine::STAGE_TOP_RATIO;
		Utils::fillRect(Engine::STAGE_TARGET_WIDTH, Engine::STAGE_TARGET_HEIGHT, size.x / size.y, width, height);
		
		float aspectRatio = width / height;
		scaledAspectRatio = scaledWidth / scaledHeight;

		auto view = DirectX::XMMatrixScaling(scaledWidth, scaledHeight, 1.f) * DirectX::XMMatrixTranslation(0.f, -scrTop, 0.f);
		auto projection = Camera().getOffCenterOrthographicProjection(-width / 2, width / 2, height / 2, -height / 2);
		auto viewProjection = DirectX::XMMatrixMultiply(view, projection); 
		const auto pView = noteEffectsCamera.getViewMatrix();
		auto pProjection = noteEffectsCamera.getProjectionMatrix(aspectRatio, 0.3f, 1000.f);
		float projectionScale = std::min(aspectRatio / EFFECTS_TARGET_ASPECT, 1.f);
		pProjection = DirectX::XMMatrixScaling(projectionScale, -projectionScale, 1.f) * pProjection;

		shader->setMatrix4("projection", viewProjection);
		float currentTime = context.getTimeAtCurrentTick();

		if (previewBuffer.getWidth() != (unsigned int)size.x || previewBuffer.getHeight() != (unsigned int)size.y)
			previewBuffer.resize((unsigned int)size.x, (unsigned int)size.y);
		previewBuffer.bind();
		previewBuffer.clear();

		renderer->beginBatch();
		if (config.drawBackground)
		{
			background.setBrightness(config.pvBackgroundBrightness);
			background.draw(renderer, width, height);
		}
		drawStage(renderer);
		renderer->endBatch();

		context.scorePreviewDrawData.effectView.updateEffects(context, noteEffectsCamera, currentTime);

		shader->use();
		shader->setMatrix4("projection", viewProjection);
		renderer->beginBatch();
		drawLines(context, renderer);
		drawHoldCurves(context, renderer);
		if (config.pvStageCover != 0) {
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
		renderer->endBatchWithBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		shader->use();
		shader->setMatrix4("projection", viewProjection);
		renderer->beginBatch();

		drawHoldTicks(context, renderer);
		drawNotes(context, renderer);
		if (config.pvStageCover != 0) {
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
		renderer->endBatchWithBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		drawList->AddImage((ImTextureID)(size_t)previewBuffer.getTexture(), position, position + size, {0, 0}, {1, 1});
	}

	void ScorePreviewWindow::updateUI(ScoreEditorTimeline& timeline, ScoreContext& context)
	{
		updateToolbar(timeline, context);
		ImGuiIO io = ImGui::GetIO();
		float mouseWheel = io.MouseWheel * 1;
		if (!timeline.isPlaying() && ImGui::IsWindowHovered() && mouseWheel != 0)
			context.currentTick += std::max(mouseWheel * TICKS_PER_BEAT / 2, (float)-context.currentTick);
		updateScrollbar(timeline, context);

		playbackState.wasLastFramePlaying = playbackState.isPlaying;
		playbackState.isPlaying = timeline.isPlaying();

		if (isFullWindow())
		{
			if (ImGui::BeginPopupContextWindow("preview_context_menu", 1 ))
			{
				bool _fullWindow = fullWindow;
				if (ImGui::MenuItem(getString("fullscreen_preview"), NULL, &_fullWindow))
					setFullWindow(_fullWindow);

				ImGui::MenuItem(getString("preview_draw_toolbar"), NULL, &config.pvDrawToolbar);
				ImGui::MenuItem(getString("return_to_last_tick"), NULL, &config.returnToLastSelectedTickOnPause);
				ImGui::EndPopup();
			}
		}
	}

	void ScorePreviewWindow::setFullWindow(bool _fullWindow)
	{
		fullWindow = _fullWindow;
	}

	void ScorePreviewWindow::loadNoteEffects(Effect::EffectView& effectView)
	{
		int oldProfile = config.pvEffectsProfile == 1 ? 0 : 1;
		const std::string oldEffectsDir = Application::getAppDir() + "res\\effect\\" + std::to_string(oldProfile) + "\\";
		const std::string effectsDir = Application::getAppDir() + "res\\effect\\" + std::to_string(config.pvEffectsProfile) + "\\";
		size_t effectCount = arrayLength(Effect::effectNames);

		ResourceManager::removeAllParticleEffects();
		int texIndex = ResourceManager::getTextureByFilename(oldEffectsDir + "tex_note_common_all_v2.png");
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

			IO::messageBox(
				APP_NAME,
				fullErrorMessage,
				IO::MessageBoxButtons::Ok,
				IO::MessageBoxIcon::Warning,
				Application::windowState.windowHandle
			);
		}

		effectView.reset();
		effectView.init();
	}

	const Texture &ScorePreviewWindow::getNoteTexture()
	{
		return ResourceManager::textures[noteTextures.notes];
	}

	void ScorePreviewWindow::drawStage(Renderer* renderer)
	{
		int index = ResourceManager::getTexture("stage");
		if (index == -1) return;
		const Texture& stage = ResourceManager::textures[index];
		if (!isArrayIndexInBounds(SPR_SEKAI_STAGE, stage.sprites)) return;
		const Sprite& stageSprite = stage.sprites[SPR_SEKAI_STAGE];
		constexpr float stageWidth = (Engine::STAGE_TEX_WIDTH / Engine::STAGE_LANE_WIDTH) * Engine::STAGE_NUM_LANES;
		constexpr float stageLeft  = -stageWidth / 2;
		constexpr float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		constexpr float stageHeight = Engine::STAGE_TEX_HEIGHT / Engine::STAGE_LANE_HEIGHT;
		
		renderer->drawRectangle(Vector2(stageLeft, stageTop), Vector2(stageWidth, stageHeight), stage, stageSprite.getX(), stageSprite.getX() + stageSprite.getWidth(), stageSprite.getY(), stageSprite.getY() + stageSprite.getHeight(), Color(defaultTint.r, defaultTint.g, defaultTint.b, defaultTint.a * config.pvStageOpacity), -1);
	}

	void ScorePreviewWindow::drawStageCoverMask(Renderer* renderer)
	{
		int index = ResourceManager::getTexture("stage");
		if (index == -1) return;
		const Texture& stage = ResourceManager::textures[index];
		constexpr float stageWidth = (Engine::STAGE_TEX_WIDTH / Engine::STAGE_LANE_WIDTH) * Engine::STAGE_NUM_LANES;
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
		if (index == -1) return;
		const Texture& stage = ResourceManager::textures[index];
		if (!isArrayIndexInBounds(SPR_SEKAI_STAGE, stage.sprites)) return;
		const Sprite& stageSprite = stage.sprites[SPR_SEKAI_STAGE];
		constexpr float stageWidth = (Engine::STAGE_TEX_WIDTH / Engine::STAGE_LANE_WIDTH) * Engine::STAGE_NUM_LANES;
		const float stageLeft = -stageWidth / 2, stageRight = stageWidth / 2;

		constexpr float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const float stageHeight = config.pvStageCover * (1 - stageTop);
		const float spriteHeight = config.pvStageCover * (Engine::STAGE_LANE_HEIGHT - Engine::STAGE_LANE_TOP);

		static auto model = DirectX::XMMatrixTranslation(0, 0, 1);
		
		float texW = (float)stage.getWidth();
		float texH = (float)stage.getHeight();
		auto vPos = Engine::quadvPos(stageLeft, stageRight, stageTop + stageHeight, stageTop);
		auto uv = Utils::getUV(stageSprite.getX() / texW, (stageSprite.getX() + stageSprite.getWidth()) / texW, stageSprite.getY() / texH, (stageSprite.getY() + spriteHeight) / texH);

		renderer->pushQuad(vPos, uv, model, DirectX::XMFLOAT4(0, 0, 0, config.pvStageOpacity), (int)stage.getID(), 0);
	}

	void ScorePreviewWindow::drawStageCoverDecoration(Renderer *renderer)
	{
		if (noteTextures.notes == -1)
			return;

		constexpr float stageTop = Engine::STAGE_LANE_TOP / Engine::STAGE_LANE_HEIGHT;
		const Texture& noteTex = getNoteTexture();
		size_t sprIndex = SPR_SIMULTANEOUS_CONNECTION;
		size_t transIndex = static_cast<size_t>(SpriteType::SimultaneousLine);
		if (!isArrayIndexInBounds(sprIndex, noteTex.sprites)) return;
		if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms)) return;

		const SpriteTransform& lineTransform = ResourceManager::spriteTransforms[transIndex];
		const Sprite& sprite = noteTex.sprites[sprIndex];
		float x = 0.12f * (1.f - config.pvStageCover);
		auto vPos = lineTransform.apply(Engine::perspectiveQuadvPos(-6.f - x, 6.f + x, 1.f + Engine::getNoteHeight(), 1.f - Engine::getNoteHeight()));
		float y = stageTop + config.pvStageCover * (1.f - stageTop);
		auto uv = Engine::quadUV(sprite, noteTex);
		auto model = DirectX::XMMatrixScaling(y, y, 1.f);
		renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, config.pvStageOpacity), (int)noteTex.getID(), -1);
	}

	void ScorePreviewWindow::drawNotes(const ScoreContext& context, Renderer *renderer)
	{
		double current_tm = accumulateDuration(context.currentTick, TICKS_PER_BEAT, context.score.tempoChanges);
		const auto layer_stm = getCurrentLayerScaledTimes(context);

		const auto& drawData = context.scorePreviewDrawData;

		for (auto& note : drawData.drawingNotes)
		{
		// 修正：at() を find() に変えて、データが存在するかチェックする
		auto it = context.score.notes.find(note.refID);
		if (it == context.score.notes.end()) continue; // 見つからない場合は描画をスキップ

		const Note& noteData = it->second;

		if (context.currentTick > noteData.tick)
			continue;

			int layer = std::clamp(noteData.layer, 0, (int)context.score.layers.size() - 1);
			double scaled_tm = layer_stm[layer];

			if (scaled_tm < note.visualTime.min)
				continue;

			double y = Engine::approach(note.visualTime.min, note.visualTime.max, scaled_tm);
			
			float l = Engine::laneToLeft(noteData.lane), r = Engine::laneToLeft(noteData.lane) + noteData.width;
			drawNoteBase(renderer, noteData, l, r, (float)y);
			if (noteData.friction)
				drawTraceDiamond(renderer, noteData, l, r, (float)y);
			if (noteData.isFlick()) 
				drawFlickArrow(renderer, noteData, (float)y, current_tm);
		}
	}

	void ScorePreviewWindow::drawLines(const ScoreContext& context, Renderer* renderer)
{
	if (!config.pvSimultaneousLine || noteTextures.notes == -1) return;
	
	const auto& drawData = context.scorePreviewDrawData.drawingLines;

	const Texture& texture = getNoteTexture();
	size_t sprIndex = SPR_SIMULTANEOUS_CONNECTION;
	if (!isArrayIndexInBounds(sprIndex, texture.sprites)) return;
	const Sprite& sprite = texture.sprites[sprIndex];

	// ★ 純正通り SimultaneousLine を使用
	size_t transIndex = static_cast<size_t>(SpriteType::SimultaneousLine);
	if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms)) return;
	const SpriteTransform& lineTransform = ResourceManager::spriteTransforms[transIndex];
	
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

		double current_left_stm = getCachedLayerScaledTime(context, context.currentTick, line.leftLayer);
		double current_right_stm = getCachedLayerScaledTime(context, context.currentTick, line.rightLayer);

		double left_progress = 1.0 - (left_stm - current_left_stm) / noteDuration;
		double right_progress = 1.0 - (right_stm - current_right_stm) / noteDuration;

		if (left_progress < 0.0 && right_progress < 0.0) continue;
		if ((left_progress < 1.0 && 1.0 < right_progress) || (left_progress > 1.0 && 1.0 > right_progress)) continue;

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

		float adj_left_travel = Engine::approachProgress(adj_left_progress);
		float adj_right_travel = Engine::approachProgress(adj_right_progress);

		if (std::abs(adj_left_lane - adj_right_lane) < 1e-6 && std::abs(adj_left_travel - adj_right_travel) < 1e-6)
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

		// ★ 純正の `auto vPos = lineTransform.apply(Engine::perspectiveQuadvPos(...));` を完全再現
		auto rawPos = Engine::perspectiveQuadvPos(noteLeft, noteRight, noteTop, noteBottom);
		auto vPos = lineTransform.apply(rawPos);

		// ★ 純正の `DirectX::XMMatrixScaling(y, y, 1.f)` を、左右独立して適用
		vPos[0].x *= adj_right_travel; vPos[0].y *= adj_right_travel;
		vPos[1].x *= adj_right_travel; vPos[1].y *= adj_right_travel;
		
		vPos[2].x *= adj_left_travel;  vPos[2].y *= adj_left_travel;
		vPos[3].x *= adj_left_travel;  vPos[3].y *= adj_left_travel;

		auto uv = Utils::getUV(
			sprite.getX() / texW, 
			(sprite.getX() + sprite.getWidth()) / texW, 
			sprite.getY() / texH, 
			(sprite.getY() + sprite.getHeight()) / texH
		);
		
		// ★ 純正の Z-Index と Tint を完全再現
		float center_y = (adj_left_travel + adj_right_travel) / 2.0f;
		int zIndex = Engine::getZIndex(SpriteLayer::UNDER_NOTE_EFFECT, 0, center_y);

		renderer->pushQuad(vPos, uv, DirectX::XMMatrixIdentity(), toFloat4(defaultTint), (int)texture.getID(), zIndex);
	}
}

	void ScorePreviewWindow::drawHoldTicks(const ScoreContext &context, Renderer *renderer)
	{
		if (noteTextures.notes == -1) return;
		const auto layer_stm = getCurrentLayerScaledTimes(context);

		const float notesHeight = Engine::getNoteHeight() * 1.3f;
		const float w = notesHeight / scaledAspectRatio;
		const float noteTop = 1. + notesHeight, noteBottom = 1. - notesHeight;
		const Texture& texture = getNoteTexture();
		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();

		size_t transIndex = static_cast<size_t>(SpriteType::HoldTick);
		if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms)) return;
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

			float y = (float)Engine::approach(tick.visualTime.min, tick.visualTime.max, scaled_tm);
			
			//  Y座標クリッピング
			if (y < -0.1 || y > 1.2) continue;

			int sprIndex = getNoteSpriteIndex(noteData);
			if (!isArrayIndexInBounds(sprIndex, texture.sprites)) continue;
			const Sprite& sprite = texture.sprites[sprIndex];
			const float tickCenter = tick.center * (config.pvMirrorScore ? -1 : 1);
			
			auto vPos = transform.apply(Engine::quadvPos(tickCenter - w, tickCenter + w, noteTop, noteBottom));
			auto uv = Utils::getUV(sprite.getX() / texW, (sprite.getX() + sprite.getWidth()) / texW, sprite.getY() / texH, (sprite.getY() + sprite.getHeight()) / texH);
			
			renderer->pushQuad(vPos, uv, DirectX::XMMatrixScaling(y, y, 1.f), toFloat4(defaultTint), (int)texture.getID(), Engine::getZIndex(SpriteLayer::DIAMOND, tickCenter, y));
		}
	}

	void ScorePreviewWindow::drawHoldCurves(const ScoreContext& context, Renderer* renderer)
	{
		const float total_tm = accumulateDuration(context.scorePreviewDrawData.maxTicks, TICKS_PER_BEAT, context.score.tempoChanges);
		const double current_tm = accumulateDuration(context.currentTick, TICKS_PER_BEAT, context.score.tempoChanges);
		const float noteDuration = Engine::getNoteDuration(config.pvNoteSpeed);
		const float mirror = config.pvMirrorScore ? -1 : 1;
		const auto& drawData = context.scorePreviewDrawData;
		const auto layer_stm = getCurrentLayerScaledTimes(context);
	
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
			// ★ 完全修正版：STM補間（曲線の維持）と、正確なアンカー固定（途切れ防止）のハイブリッド
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
	
			double stm_top = current_stm + 1.0 * noteDuration;
			double stm_bottom = current_stm - 0.2 * noteDuration;
	
			// 画面に映る範囲のみを計算するカリング処理（元のコードの考え方と同じ）
			double p_view_a = unlerpD(start_stm, segment.tailTime, stm_bottom);
			double p_view_b = unlerpD(start_stm, segment.tailTime, stm_top);
			double p_min = std::clamp(std::min(p_view_a, p_view_b), 0.0, 1.0);
			double p_max = std::clamp(std::max(p_view_a, p_view_b), 0.0, 1.0);
	
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
				const HoldNote& hold = context.score.holdNotes.at(holdStart.ID);
				sprIndex = (int)hold.guideColor;
			}
			else
			{
				textureID = noteTextures.holdPath;
				sprIndex = (!holdStart.critical ? 1 : 3);
			}
	
			if (textureID == -1) continue;
			const Texture& texture = ResourceManager::textures[textureID];
			if (!isArrayIndexInBounds(sprIndex, texture.sprites)) continue;
			const Sprite& segmentSprite = texture.sprites[sprIndex];
	
			const auto ease = getEaseFunction(segment.ease);
			float startLeft = segment.headLeft, startRight = segment.headRight, endLeft = segment.tailLeft, endRight = segment.tailRight;
	
			// 分割数の計算 (元のSTMを用いたロジックを維持)
			double start_y = Engine::approach(start_stm - noteDuration, start_stm, current_stm);
			double end_y   = Engine::approach(segment.tailTime - noteDuration, segment.tailTime, current_stm);
	
			int steps = 10;
			if (segment.ease == EaseType::Linear)
			{
				double mid_travel = (start_y + end_y) / 2.0;
				double perspective_factor = std::pow(std::max(0.1, mid_travel), 0.8);
				double x_diff_max = std::max(std::abs(startLeft - endLeft), std::abs(startRight - endRight));
				
				// Xの移動量計算には時間割合(time_frac)を使う
				double t_frac_start = unlerpD(segment.startTime, segment.endTime, start_time);
				double t_frac_end   = 1.0; 
				double x_diff = (x_diff_max * 2.5 / perspective_factor) * std::abs(t_frac_end - t_frac_start);
				double curve_change_scale = std::pow(x_diff, 0.8);
				steps = std::max(1, static_cast<int>(std::ceil(curve_change_scale * 10.0)));
			}
			else
			{
				double pos_offset = 0.0;
				double ref_start_lane = std::abs(startLeft - endLeft) > std::abs(startRight - endRight) ? startLeft : startRight;
				double ref_end_lane = std::abs(startLeft - endLeft) > std::abs(startRight - endRight) ? endLeft : endRight;
				
				double t_frac_start = unlerpD(segment.startTime, segment.endTime, start_time);
				double t_frac_end   = 1.0; 
	
				double pos_offset_this_side = 0.0;
				for (double r : {0.25, 0.75}) {
					double time_frac = lerpD(t_frac_start, t_frac_end, r);
					double interp_frac = ease(0.0f, 1.0f, (float)time_frac); 
					double y = lerpD(start_y, end_y, r);
					double lane = lerpD(ref_start_lane, ref_end_lane, interp_frac);
					double ref_pos = lerpD(ref_start_lane, ref_end_lane, r);
					double screen_offset = std::abs(lane - ref_pos);
					double compensation_factor = std::pow(std::max(0.1, y), 0.8);
					pos_offset_this_side += screen_offset / compensation_factor;
				}
				pos_offset = pos_offset_this_side * std::pow(std::abs(t_frac_end - t_frac_start), 0.7);
				double curve_change_scale = std::pow(pos_offset, 0.4) * 2.0;
				steps = std::max(1, static_cast<int>(std::ceil(curve_change_scale * 10.0)));
			}
			steps = std::clamp(steps, 1, 200);
	
			// ==============================================================================
	
			if (isSegmentActivated && context.score.holdNotes.at(holdStart.ID).startType == HoldNoteType::Normal)
			{
				double base_frac = unlerpD(segment.startTime, segment.endTime, start_time);
				float l = ease(startLeft, endLeft, (float)base_frac), r = ease(startRight, endRight, (float)base_frac);
				drawNoteBase(renderer, holdStart, l, r, 1.f, segment.activeTime / total_tm);
				if (holdStart.friction) drawTraceDiamond(renderer, holdStart, l, r, 1.f);
			}
	
			if (config.pvMirrorScore)
			{
				std::swap(startLeft *= -1, startRight *= -1);
				std::swap(endLeft *= -1, endRight *= -1);
			}
	
			double holdStartProgress, holdEndProgress;
			if (segment.isGuide)
			{
				const HoldNote& hold = context.score.holdNotes.at(holdStart.ID);
				double totalJoints = 1 + hold.steps.size();
				double headProgress = segment.tailStepIndex / totalJoints;
				double tailProgress = (segment.tailStepIndex + 1) / totalJoints;
	
				double base_frac = unlerpD(segment.startTime, segment.endTime, start_time);
				holdStartProgress = lerpD(headProgress, tailProgress, base_frac);
				holdEndProgress = lerpD(headProgress, tailProgress, 1.0);
			}
	
			double from_percentage = 0;
			
			// ループ初期値の設定
			double stepStart_stm = lerpD(start_stm, segment.tailTime, p_min);
			double stepTop = Engine::approach(stepStart_stm - noteDuration, stepStart_stm, current_stm);
	
			double stepStart_time = lerpD(start_time, segment.endTime, p_min);
			double stepStart_timeFrac = unlerpD(segment.startTime, segment.endTime, stepStart_time);
	
			auto model = DirectX::XMMatrixIdentity();
			float baseAlpha = segment.isGuide ? config.pvGuideAlpha : config.pvHoldAlpha;
			int zIndex = Engine::getZIndex(segment.isGuide ? SpriteLayer::GUIDE_PATH : SpriteLayer::HOLD_PATH, holdStartCenter, segment.activeTime / total_tm);
	
			for (int i = 0; i < steps; i++)
			{
				double to_p = lerpD(p_min, p_max, (double)(i + 1) / steps);
				
				// Y座標用には「STM」を補間して使う（純正コードの美しい曲線を維持）
				double stepEnd_stm = lerpD(start_stm, segment.tailTime, to_p);
				double stepBottom = Engine::approach(stepEnd_stm - noteDuration, stepEnd_stm, current_stm);
	
				// X座標用には「時間割合」を補間して使う（レーンの移動が時間ベースで正確になる）
				double stepEnd_time = lerpD(start_time, segment.endTime, to_p);
				double stepEnd_timeFrac = unlerpD(segment.startTime, segment.endTime, stepEnd_time);
	
				float stepStartLeft = ease(startLeft, endLeft, (float)stepStart_timeFrac);
				float   stepEndLeft = ease(startLeft, endLeft, (float)stepEnd_timeFrac);
				float stepStartRight = ease(startRight, endRight, (float)stepStart_timeFrac);
				float   stepEndRight = ease(startRight, endRight, (float)stepEnd_timeFrac);
	
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
	
				auto vPos = Engine::perspectiveQuadvPos(q_leftStart, q_leftStop, q_rightStart, q_rightStop, q_top, q_bottom);
				// =======================================================================================
	
				float spr_x1, spr_x2, spr_y1, spr_y2;
				std::array<DirectX::XMFLOAT4, 4> vertexColors;
	
				if (segment.isGuide)
				{
					const HoldNote& hold = context.score.holdNotes.at(holdStart.ID);
					double startProg = lerpD(holdStartProgress, holdEndProgress, from_percentage);
					double to_percentage = double(i + 1) / steps;
					double endProg = lerpD(holdStartProgress, holdEndProgress, to_percentage);
					float startAlpha = baseAlpha;
					float endAlpha = baseAlpha;
					if (hold.fadeType == FadeType::Out) {
						startAlpha *= (1.0f - (float)startProg);
						endAlpha *= (1.0f - (float)endProg);
					} else if (hold.fadeType == FadeType::In) {
						startAlpha *= (float)startProg;
						endAlpha *= (float)endProg;
					}
					vertexColors = {{
						toFloat4(defaultTint, startAlpha),
						toFloat4(defaultTint, endAlpha),
						toFloat4(defaultTint, endAlpha),
						toFloat4(defaultTint, startAlpha)
					}};
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
	
				if (config.pvHoldAnimation && isHoldActivated && !segment.isGuide && isArrayIndexInBounds(sprIndex - 1, texture.sprites))
				{
					const Sprite& activeSprite = texture.sprites[sprIndex - 1];
					const int norm2ActiveOffset = (int)(activeSprite.getY() - segmentSprite.getY());
					double delta_tm = current_tm - segment.activeTime;
					float normalAplha = (std::cos((float)delta_tm * MATH_PI * 2.f) + 2.f) / 3.f;
					renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, baseAlpha * normalAplha), (int)texture.getID(), zIndex);
					auto uvActive = Utils::getUV(spr_x1 / texW, spr_x2 / texW, (spr_y1 + norm2ActiveOffset) / texH, (spr_y2 + norm2ActiveOffset) / texH);
					renderer->pushQuad(vPos, uvActive, model, toFloat4(defaultTint, baseAlpha * (1.f - normalAplha)), (int)texture.getID(), zIndex);
				}
				else if (segment.isGuide)
				{
					renderer->pushQuad(vPos, uv, model, vertexColors, (int)texture.getID(), zIndex);
				}
				else
				{
					renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, baseAlpha), (int)texture.getID(), zIndex);
				}
				
				// ループ終端の更新処理
				stepTop = stepBottom;
				stepStart_timeFrac = stepEnd_timeFrac;
			}
		}
	}

	void ScorePreviewWindow::drawNoteBase(Renderer* renderer, const Note& note, float noteLeft, float noteRight, float y, float zScalar)
	{
		int textureID = note.getType() == NoteType::Damage ? noteTextures.ccNotes : noteTextures.notes;
		if (textureID == -1) return;
		const Texture& texture = ResourceManager::textures[textureID];

		const int sprIndex = note.getType() == NoteType::Damage ? getCcNoteSpriteIndex(note) : getNoteSpriteIndex(note);
		if (!isArrayIndexInBounds(sprIndex, texture.sprites)) return;
		const Sprite& sprite = texture.sprites[sprIndex];

		size_t transIndexM = static_cast<size_t>(SpriteType::NoteMiddle);
		size_t transIndexL = static_cast<size_t>(SpriteType::NoteLeft);
		size_t transIndexR = static_cast<size_t>(SpriteType::NoteRight);
		if (!isArrayIndexInBounds(transIndexM, ResourceManager::spriteTransforms) ||
			!isArrayIndexInBounds(transIndexL, ResourceManager::spriteTransforms) ||
			!isArrayIndexInBounds(transIndexR, ResourceManager::spriteTransforms)) return;

		const SpriteTransform& mTransform = ResourceManager::spriteTransforms[transIndexM];
		const SpriteTransform& lTransform = ResourceManager::spriteTransforms[transIndexL];
		const SpriteTransform& rTransform = ResourceManager::spriteTransforms[transIndexR];

		const float noteHeight = Engine::getNoteHeight();
		const float noteTop = 1.f - noteHeight, noteBottom = 1.f + noteHeight;
		if (config.pvMirrorScore) std::swap(noteLeft *= -1.f, noteRight *= -1.f);
		int zIndex = Engine::getZIndex(!note.friction ? SpriteLayer::BASE_NOTE : SpriteLayer::TICK_NOTE, noteLeft + (noteRight - noteLeft) / 2.f, y * zScalar);

		auto model = DirectX::XMMatrixScaling(y, y, 1.f);
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

		if (geomWidth > 0.0f)
		{
			vPos = mTransform.apply(Engine::perspectiveQuadvPos(middleLeft, middleRight, noteTop, noteBottom));
			uv = Utils::getUV(midUvLeft / texW, midUvRight / texW, sprite.getY() / texH, (sprite.getY() + sprite.getHeight()) / texH);
			renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint), (int)texture.getID(), zIndex);
		}

		// Left slice (純正完全維持)
		vPos = lTransform.apply(Engine::perspectiveQuadvPos(noteLeft, noteLeft + 0.25f, noteTop, noteBottom));
		uv = Utils::getUV((sprite.getX() + NOTE_SIDE_PAD) / texW, (sprite.getX() + NOTE_SIDE_WIDTH) / texW, sprite.getY() / texH, (sprite.getY() + sprite.getHeight()) / texH);
		renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint), (int)texture.getID(), zIndex);
		
		// Right slice (純正完全維持)
		vPos = rTransform.apply(Engine::perspectiveQuadvPos(noteRight - 0.3f, noteRight, noteTop, noteBottom));
		uv = Utils::getUV((sprite.getX() + sprite.getWidth() - NOTE_SIDE_WIDTH) / texW, (sprite.getX() + sprite.getWidth() - NOTE_SIDE_PAD) / texW, sprite.getY() / texH, (sprite.getY() + sprite.getHeight()) / texH);
		renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint), (int)texture.getID(), zIndex);
	}

	void ScorePreviewWindow::drawTraceDiamond(Renderer *renderer, const Note &note, float noteLeft, float noteRight, float y)
	{
		if (noteTextures.notes == -1) return;
		const Texture& texture = getNoteTexture();
		int frictionSprIndex = getFrictionSpriteIndex(note);
		if (!isArrayIndexInBounds(frictionSprIndex, texture.sprites)) return;
		const Sprite& frictionSpr = texture.sprites[frictionSprIndex];

		size_t transIndex = static_cast<size_t>(SpriteType::TraceDiamond);
		if (!isArrayIndexInBounds(transIndex, ResourceManager::spriteTransforms)) return;
		const SpriteTransform& transform = ResourceManager::spriteTransforms[transIndex];

		const float w = Engine::getNoteHeight() / scaledAspectRatio;
		const float noteTop = 1.f + Engine::getNoteHeight(), noteBottom = 1.f - Engine::getNoteHeight();
		if (config.pvMirrorScore) std::swap(noteLeft *= -1.f, noteRight *= -1.f);
		const float noteCenter = noteLeft + (noteRight - noteLeft) / 2.f;
		int zIndex = Engine::getZIndex(SpriteLayer::DIAMOND, noteCenter, y);

		auto vPos = transform.apply(Engine::quadvPos(noteCenter - w, noteCenter + w, noteTop, noteBottom));
		
		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();
		auto uv = Utils::getUV(frictionSpr.getX() / texW, (frictionSpr.getX() + frictionSpr.getWidth()) / texW, frictionSpr.getY() / texH, (frictionSpr.getY() + frictionSpr.getHeight()) / texH);
		
		auto model = DirectX::XMMatrixScaling(y, y, 1.f);
		renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint), (int)texture.getID(), zIndex);
	}

	void ScorePreviewWindow::drawFlickArrow(Renderer *renderer, const Note &note, float y, double time)
	{
		if (noteTextures.notes == -1) return;
		const Texture& texture = getNoteTexture();
		const int sprIndex = getFlickArrowSpriteIndex(note);
		if (!isArrayIndexInBounds(sprIndex, texture.sprites)) return;
		const Sprite& arrowSprite = texture.sprites[sprIndex];

		//  DownLeft, DownRight も左右フリックとして扱うように判定を追加
		bool isLeftOrRight = (note.flick == FlickType::Left || note.flick == FlickType::Right || note.flick == FlickType::DownLeft || note.flick == FlickType::DownRight);
		bool isRightward = (note.flick == FlickType::Right || note.flick == FlickType::DownRight);

		size_t flickTransformIdx = std::clamp((int)note.width, 1, MAX_FLICK_SPRITES) - 1 + static_cast<int>(isLeftOrRight ? SpriteType::FlickArrowLeft : SpriteType::FlickArrowUp);
		if (!isArrayIndexInBounds(flickTransformIdx, ResourceManager::spriteTransforms)) return;
		const SpriteTransform& transform = ResourceManager::spriteTransforms[flickTransformIdx];

		const int mirror = config.pvMirrorScore ? -1 : 1;
		const int flickDirection = mirror * (isLeftOrRight ? (isRightward ? 1 : -1) : 0);
		const float center = Engine::getNoteCenter(note) * mirror;
		const float w = std::clamp((int)note.width, 1, MAX_FLICK_SPRITES) * (isRightward ? -1.f : 1.f) * mirror / 4.f;
		
		auto vPos = transform.apply(Engine::quadvPos(center - w, center + w, 1.f, 1.f - 2.f * std::abs(w) * scaledAspectRatio));
		
		float texW = (float)texture.getWidth();
		float texH = (float)texture.getHeight();
		auto uv = Utils::getUV(arrowSprite.getX() / texW, (arrowSprite.getX() + arrowSprite.getWidth()) / texW, arrowSprite.getY() / texH, (arrowSprite.getY() + arrowSprite.getHeight()) / texH);
		
		//  下フリックの場合、UV座標のY軸を入れ替えて画像を上下反転させる
		bool isDown = (note.flick >= FlickType::Down && note.flick <= FlickType::DownRight);
		if (isDown)
		{
			// Utils::getUV は {右上, 右下, 左下, 左上} の順なので、0と1(右側)、3と2(左側)のY座標を入れ替える
			std::swap(uv[0].y, uv[1].y);
			std::swap(uv[3].y, uv[2].y);
		}

		int zIndex = Engine::getZIndex(SpriteLayer::FLICK_ARROW, center, y);
		
		if (config.pvFlickAnimation)
		{
			double t = std::fmod(time, 0.5) / 0.5;
			auto cubicEaseIn = [](double val) { return (float)(val * val * val); };
			auto animationVector = DirectX::XMVectorScale(DirectX::XMVectorSet((float)flickDirection, -2.f * scaledAspectRatio, 0.f, 0.f), (float)t);
			auto model = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranslationFromVector(animationVector), DirectX::XMMatrixScaling(y, y, 1.f));
			renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint, 1.f - cubicEaseIn(t)), (int)texture.getID(), zIndex);
		}
		else
		{
			auto model = DirectX::XMMatrixScaling(y, y, 1.f);
			renderer->pushQuad(vPos, uv, model, toFloat4(defaultTint), (int)texture.getID(), zIndex);
		}
	}

	void ScorePreviewWindow::updateToolbar(ScoreEditorTimeline &timeline, ScoreContext &context) const
	{
		static float lastHoveredTime = -1;
		constexpr float MAX_NO_HOVER_TIME = 1.5f;
		static float toolBarWidth = UI::btnNormal.x * 2;
		if (!config.pvDrawToolbar) return;
		ImGuiIO io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImVec2{
			ImGui::GetContentRegionAvail().x - ImGui::GetStyle().WindowPadding.x * 4 - toolBarWidth,
			ImGui::GetStyle().WindowPadding.y * 5
		});
		ImGui::SetNextWindowSizeConstraints({48, 0}, {120, FLT_MAX}, NULL);
		auto easeInCubic = [](float t) { return t * t * t; };
		float childBgAlpha = std::clamp(easeInCubic(unlerp(MAX_NO_HOVER_TIME, 0.f, lastHoveredTime)), 0.25f, 1.f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetColorU32(ImGuiCol_WindowBg, childBgAlpha));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
		
		ImGui::Begin("###preview_toolbar", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_ChildWindow);
		toolBarWidth = ImGui::GetWindowWidth();
		float centeredXBtn = toolBarWidth / 2 - UI::btnNormal.x / 2;
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) lastHoveredTime = 0;
		else lastHoveredTime = std::min(io.DeltaTime + lastHoveredTime, MAX_NO_HOVER_TIME);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_DOUBLE_UP, UI::btnNormal, true, context.currentTick < context.scorePreviewDrawData.maxTicks + TICKS_PER_BEAT))
		{
			if (timeline.isPlaying()) timeline.setPlaying(context, false);
			context.currentTick = timeline.roundTickDown(context.currentTick, timeline.getDivision()) + (TICKS_PER_BEAT / (timeline.getDivision() / 4));
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_UP, UI::btnNormal, true, context.currentTick < context.scorePreviewDrawData.maxTicks + TICKS_PER_BEAT))
		{
			if (timeline.isPlaying()) timeline.setPlaying(context, false);
			context.currentTick++;
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_STOP, UI::btnNormal, false)) timeline.stop(context);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(timeline.isPlaying() ? ICON_FA_PAUSE : ICON_FA_PLAY, UI::btnNormal)) timeline.setPlaying(context, !timeline.isPlaying());

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_DOWN, UI::btnNormal, true, context.currentTick > 0))
		{
			if (timeline.isPlaying()) timeline.setPlaying(context, false);
			context.currentTick--;
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_ANGLE_DOUBLE_DOWN, UI::btnNormal, true, context.currentTick > 0))
		{
			if (timeline.isPlaying()) timeline.setPlaying(context, false);
			context.currentTick = std::max(timeline.roundTickDown(context.currentTick, timeline.getDivision()) - (TICKS_PER_BEAT / (timeline.getDivision() / 4)), 0);
		}

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(isFullWindow() ? ICON_FA_COMPRESS : ICON_FA_EXPAND)) fullWindow = !isFullWindow();

		ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_MINUS, UI::btnNormal, false, timeline.getPlaybackSpeed() > 0.25f)) timeline.setPlaybackSpeed(context, timeline.getPlaybackSpeed() - 0.25f);

		const float playbackStrWidth = ImGui::CalcTextSize("0000%").x;
		ImGui::SetCursorPosX(toolBarWidth / 2 - playbackStrWidth / 2);
		UI::transparentButton(IO::formatString("%.0f%%", timeline.getPlaybackSpeed() * 100).c_str(), ImVec2{playbackStrWidth, UI::btnNormal.y }, false, false);

		ImGui::SetCursorPosX(centeredXBtn);
		if (UI::transparentButton(ICON_FA_PLUS, UI::btnNormal, false, timeline.getPlaybackSpeed() < 1.0f)) timeline.setPlaybackSpeed(context, timeline.getPlaybackSpeed() + 0.25f);

		ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

		float currentTm = accumulateDuration(context.currentTick, TICKS_PER_BEAT, context.score.tempoChanges);
		
		//  ツールバーの表示用には、現在選択されているレイヤーの視覚的時間を用いる
		int currentLayer = std::clamp(context.selectedLayer, 0, (int)context.score.layers.size() - 1);
		double currentScaledTm = getCachedLayerScaledTime(context, context.currentTick, currentLayer);
		int currentMeasure = accumulateMeasures(context.currentTick, TICKS_PER_BEAT, context.score.timeSignatures);
		const TimeSignature& ts = context.score.timeSignatures[findTimeSignature(currentMeasure, context.score.timeSignatures)];
		const Tempo& tempo = getTempoAt(context.currentTick, context.score.tempoChanges);
		int hiSpeedIdx = findHighSpeedChange(context.currentTick, context.score.hiSpeedChanges, 0);
		float speed = (hiSpeedIdx == -1 ? 1.0f : context.score.hiSpeedChanges[hiSpeedIdx].speed);

		char rhythmString[256];
		snprintf(rhythmString, sizeof(rhythmString), "%02d:%02d:%02d|%.2fs|%d/%d|%g BPM|%.2fx",
			static_cast<int>(currentTm / 60), static_cast<int>(std::fmod(currentTm, 60.f)), static_cast<int>(std::fmod(currentTm * 100, 100.f)),
			currentScaledTm,
			ts.numerator, ts.denominator,
			tempo.bpm,
			speed
		);
		char* str = strtok(rhythmString, "|");
		ImGui::SetCursorPosX(toolBarWidth / 2 - ImGui::CalcTextSize(str).x / 2);
		ImGui::Text(str);
		for (auto&& col : {feverColor, timeColor, tempoColor, speedColor})
		{
			str = strtok(NULL, "|");
			ImGui::SetCursorPosX(toolBarWidth / 2 - ImGui::CalcTextSize(str).x / 2);
			ImGui::TextColored(ImColor(col), str);
		}
		ImGui::EndChild();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar();
	}

	float ScorePreviewWindow::getScrollbarWidth() const { return ImGui::GetStyle().ScrollbarSize + 4.f; }

	void ScorePreviewWindow::updateScrollbar(ScoreEditorTimeline &timeline, ScoreContext &context) const
	{
		constexpr float scrollpadY = 30.f;
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 contentSize = ImGui::GetWindowContentRegionMax();
		ImVec2 cursorBegPos = ImGui::GetCursorStartPos();
		ImVec2 scrollbarSize = { getScrollbarWidth(), contentSize.y - cursorBegPos.y };
		
		ImGui::SetCursorPos(cursorBegPos + ImVec2{ contentSize.x - scrollbarSize.x - style.WindowPadding.x / 2, 0 });
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarBg));
		ImGui::BeginChild("###scrollbar", scrollbarSize, false, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
		ImGui::PopStyleColor();

		ImVec2 scrollContentSize = ImGui::GetContentRegionAvail();
		ImVec2 scrollMaxSize = ImGui::GetWindowContentRegionMax();
		int maxTicks = std::max(context.scorePreviewDrawData.maxTicks, 1);

		float scrollRatio = std::min((float)(Engine::getNoteDuration(config.pvNoteSpeed) / accumulateDuration(context.scorePreviewDrawData.maxTicks, TICKS_PER_BEAT, context.score.tempoChanges)), 1.f);
		
		float progress = 1.f - std::min(float(context.currentTick) / maxTicks, 1.f);
		float handleHeight = std::max(20.f, scrollContentSize.y * scrollRatio);
		
		bool scrollbarActive = false;
		ImGui::BeginDisabled(timeline.isPlaying());
		ImGui::SetCursorPos(ImGui::GetCursorStartPos());
		ImGui::InvisibleButton("##scroll_bg", contentSize, ImGuiButtonFlags_NoNavFocus);
		scrollbarActive |= ImGui::IsItemActive();

		ImVec2 handleSize = {style.ScrollbarSize, handleHeight};
		ImVec2 handlePos = {scrollMaxSize.x / 2 - handleSize.x / 2, lerp(0.f, scrollMaxSize.y - handleHeight, progress)};
		ImVec2 absHandlePos = ImGui::GetWindowPos() + handlePos;

		ImGui::SetCursorPos(handlePos);
		ImGui::InvisibleButton("##scroll_handle", handleSize);
		scrollbarActive |= ImGui::IsItemActive();

		ImGuiCol_ handleColBase = scrollbarActive ? ImGuiCol_ScrollbarGrabActive : ImGui::IsItemHovered() ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab;
		
		ImGui::RenderFrame(absHandlePos, absHandlePos + ImGui::GetItemRectSize(), ImGui::GetColorU32(handleColBase), true, 3.f);
		ImGui::EndDisabled();

		if (scrollbarActive)
		{
			float absScrollStart = ImGui::GetWindowPos().y + handleSize.y / 2.f;
			float absScrollEnd = ImGui::GetWindowPos().y + scrollMaxSize.y - handleSize.y / 2.f;
			float mouseProgress = 1.f - std::clamp(unlerp(absScrollStart, absScrollEnd, io.MousePos.y), 0.f, 1.f);
			context.currentTick = (int)std::round(lerp(0.f, (float)maxTicks, mouseProgress));	
		}
		ImGui::EndChild();
	}
}
