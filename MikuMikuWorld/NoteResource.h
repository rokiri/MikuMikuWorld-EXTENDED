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
		const Sprite* getInsertModeSprite(const InsertMode& mode, const EditArgs& edit) const;

		bool load();
		const Texture* getToolbarTexture() const;

	  private:
		std::unique_ptr<Texture> toolbarTex;
	};

	class NoteTexture
	{
	  public:
		const Sprite* getNoteSprite(const Note& note) const;
		const Sprite* getFlickArrowSprite(const Note& note) const;
		const Sprite* getFrictionSprite(const Note& note) const;
		const Sprite* getDummyCrossSprite() const;

		bool load(const std::string& profile);
		const Texture* getTexture() const;

		const std::vector<std::string>& getProfiles() const;
		void scanProfiles();

	  private:
		std::unique_ptr<Texture> texture;
		std::vector<std::string> profiles;
	};
}
