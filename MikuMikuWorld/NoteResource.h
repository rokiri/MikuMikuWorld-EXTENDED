#pragma once
#include <memory>
#include <vector>
#include "Rendering/Texture.h"
#include "Rendering/Shader.h"
#include "Note.h"
#include "ScoreContext.h"

namespace MikuMikuWorld
{
	class TimelineResources
	{
	  public:
		const Sprite* getScoreStatsSprite(const InsertMode& mode);
		const Sprite* getInsertModeSprite(const InsertMode& mode, const EditArgs& edit) const;

		bool load();
		const Texture* getToolbarTexture() const;

	  private:
		std::unique_ptr<Texture> toolbarTex;
		std::vector<const Sprite*> sprites;
	};

	struct NoteMapping
	{
		float leftX, centerX, rightX, top, bottom;
	};

	class NoteResources
	{
	  public:
		const Sprite* getNoteSprite(const Note& note) const;
		const Sprite* getFlickArrowSprite(const Note& note) const;
		const Sprite* getFrictionSprite(const Note& note) const;
		const Sprite* getDummyCrossSprite() const;
		const Sprite* getHoldNoteSprite(const HoldNoteStep& holdStep) const;

		NoteMapping getTapNoteMapping(const Note& note) const;
		NoteMapping getHoldStepMapping(const HoldNoteStep& holdStep) const;

		bool load(const std::string& profile);
		const Texture* getTexture() const;

		const std::vector<std::string>& getProfiles() const;
		void scanProfiles();

	  private:
		std::unique_ptr<Texture> texture;
		std::vector<std::string> profiles;
		// Fast lookup
		std::vector<const Sprite*> sprites;
		float noteMargin, noteSideWidth;
		float holdMargin, holdSideWidth;
	};

	class BackgroundResources
	{
	  public:
		bool load();
		bool setBackground(const std::string& filename);
		bool isBackgroundSet() const;

		const Texture* getStageTexture() const;
		const Texture* getStageBGTexture() const;
		const Texture* getStageDecoTexture() const;
		const Texture* getBackgroundTexture() const;
		const Texture* getDefaultJacketTexture() const;

		const Sprite* getMaskWhite() const;
		const Sprite* getJacketLeftCover() const;
		const Sprite* getJacketRightCover() const;
		const Sprite* getJacketLeftCoverMirror() const;
		const Sprite* getJacketRightCoverMirror() const;
		const Sprite* getJacketCenterWindow() const;
		const Sprite* getJacketCenterWindowMirror() const;
		const Sprite* getJacketCenterCover() const;
		const Sprite* getJacketCenterCoverMirror() const;
		const Sprite* getStageFloor() const;
		std::array<DirectX::XMFLOAT4, 4> getJacketMaskLeftUV() const;
		std::array<DirectX::XMFLOAT4, 4> getJacketMaskRightUV() const;
		std::array<DirectX::XMFLOAT4, 4> getJacketMaskLeftMirrorUV() const;
		std::array<DirectX::XMFLOAT4, 4> getJacketMaskRightMirrorUV() const;
		std::array<DirectX::XMFLOAT4, 4> getJacketMaskCenterUV() const;
		std::array<DirectX::XMFLOAT4, 4> getJacketMaskCenterMirrorUV() const;

	  private:
		std::unique_ptr<Texture> stage;
		std::unique_ptr<Texture> stageBackground;
		std::unique_ptr<Texture> stageDecoration;
		std::unique_ptr<Texture> background;
		std::unique_ptr<Texture> jacket;
	};
}