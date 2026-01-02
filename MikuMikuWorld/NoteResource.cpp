#include "Application.h"
#include "NoteResource.h"

namespace fs = std::filesystem;

namespace MikuMikuWorld
{
	const Sprite* TimelineTexture::getInsertModeSprite(const InsertMode& mode,
	                                                   const EditArgs& edit) const
	{
		if (!toolbarTex)
			return nullptr;
		int modeIdx = static_cast<int>(mode);
		if (!isArrayIndexInBounds(modeIdx, insertModes))
			return nullptr;
		std::string name = insertModes[modeIdx];
		switch (mode)
		{
		case InsertMode::InsertLong:
			name.append("_").append(easeTypes[size_t(edit.easeType)]);
			break;
		case InsertMode::InsertFlick:
			name.append("_").append(flickTypes[size_t(edit.flickType)]);
			break;
		case InsertMode::InsertLongMid:
			name.append("_").append(stepTypes[size_t(edit.stepType)]);
			break;
		case InsertMode::InsertGuide:
			name.append("_")
			    .append(arrayGetItemSafe(guideColors, edit.colorType))
			    .append("_")
			    .append(arrayGetItemSafe(fadeTypes, edit.fadeType));
			break;
		case InsertMode::InsertHiSpeed:
			if (edit.hiSpeedHideNotes)
				name.append("_hide");
			break;
		}
		return toolbarTex->getSprite(name);
	}

	bool TimelineTexture::load()
	{
		auto path = Application::getFullPath("res", "textures", "editor", "timeline_tools.png");
		if (!fs::exists(path))
		{
			fprintf(stderr, "ERROR: Could not find texture file %s\n", path.string().c_str());
			return false;
		}
		toolbarTex = std::make_unique<Texture>(path.string());
		return true;
	}

	const Texture* TimelineTexture::getToolbarTexture() const { return toolbarTex.get(); }

	const Sprite* NoteTexture::getNoteSprite(const Note& note) const
	{
		if (!texture)
			return nullptr;
		const char* prefix = note.type == NoteType::Damage ? "cc_" : "";
		const char* name = "";

		switch (note.type)
		{
		case NoteType::Tap:
			prefix = "";
			if (!note.isTrace())
				name = note.isCrit() ? "0" : note.isFlick() ? "3" : note.isHold() ? "1" : "2";
			else
				name = note.isCrit() ? "5" : note.isFlick() ? "6" : "4";
			break;
		case NoteType::Tick:
			prefix = "";
			name = note.isCrit() ? "long_among_crtcl" : "long_among";
			break;
		case NoteType::Damage:
			prefix = "cc_";
			name = "0";
			break;
		default:
			return nullptr;
		}

		return texture->getSprite(IO::formatString("%snotes_%d", prefix, name));
	}

	const Sprite* NoteTexture::getFlickArrowSprite(const Note& note) const
	{
		if (!texture)
			return nullptr;
		using NumericType = decltype(note.width);
		const char* prefix = note.isCrit() ? "crtcl_" : "";
		int arrowSize = std::min(std::max(note.width, NumericType(1)), NumericType(6));
		return texture->getSprite(IO::formatString("notes_flick_arrow_%s%02d", prefix, arrowSize));
	}

	const Sprite* NoteTexture::getFrictionSprite(const Note& note) const
	{
		const char* name = note.isCrit()    ? "notes_friction_among_crtcl"
		                   : note.isFlick() ? "notes_friction_among_flick"
		                                    : "notes_friction_among_long";
		return texture ? texture->getSprite(name) : nullptr;
	}

	const Sprite* NoteTexture::getDummyCrossSprite() const
	{
		return texture ? texture->getSprite("ns_dummy_cross") : nullptr;
	}

	bool NoteTexture::load(const std::string& profile)
	{
		auto path = Application::getFullPath("res", "textures", "note") / profile / "notes.png";
		if (!fs::exists(path))
		{
			fprintf(stderr, "ERROR: Could not find texture file %s\n", path.string().c_str());
			return false;
		}
		texture = std::make_unique<Texture>(path.string());
		return true;
	}

	const Texture* NoteTexture::getTexture() const { return texture.get(); }

	const std::vector<std::string>& NoteTexture::getProfiles() const { return profiles; }

	void MikuMikuWorld::NoteTexture::scanProfiles()
	{
		profiles.clear();
		auto notePath = Application::getFullPath("res/textures/note");
		if (!fs::exists(notePath))
			return;
		for (auto&& entry : fs::directory_iterator(notePath))
			if (entry.is_directory())
				profiles.push_back(entry.path().filename().string());
	}
}