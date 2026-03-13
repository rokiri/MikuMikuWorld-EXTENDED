#pragma once
#include <memory>
#include <vector>
#include "Rendering/Texture.h"
#include "Rendering/Shader.h"
#include "Note.h"
#include "ScoreContext.h"

namespace MikuMikuWorld
{
	class TimelineTexture
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

	class NoteTexture
	{
	  public:
		const Sprite* getNoteSprite(const Note& note) const;
		const Sprite* getFlickArrowSprite(const Note& note) const;
		const Sprite* getFrictionSprite(const Note& note) const;
		const Sprite* getDummyCrossSprite() const;
		const Sprite* getHoldNoteSprite(const HoldNoteStep & holdStep) const;

		NoteMapping getTapNoteMapping(const Note& note) const;
		NoteMapping getHoldStepMapping(const HoldNoteStep & holdStep) const;

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
}