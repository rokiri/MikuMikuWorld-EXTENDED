#include "Application.h"
#include "NoteResource.h"

namespace fs = std::filesystem;
namespace mmw = MikuMikuWorld;

namespace MikuMikuWorld
{
	inline constexpr const char* insertModes[]{
		"timeline_select",  "timeline_tap",      "timeline_hold",  "timeline_hold_step",
		"timeline_flick",   "timeline_critical", "timeline_trace", "timeline_guide",
		"timeline_damage",  "timeline_dummy",    "timeline_bpm",   "timeline_time_signature",
		"timeline_hi_speed"
	};

	enum INS_SPR_
	{
		INS_SPR_SELECT = 0,
		INS_LONG_SPR_ = std::size(insertModes),
		INS_LONG_MID_SPR_ = INS_LONG_SPR_ + std::size(easeTypes),
		INS_FLICK_SPR_ = INS_LONG_MID_SPR_ + std::size(stepTypes),
		INS_GUIDE_SPR_ = INS_FLICK_SPR_ + std::size(flickTypes),
		INS_HISPEED_SPR_HIDE = INS_GUIDE_SPR_ + std::size(guideColors) * std::size(fadeTypes),
		INS_SPR_MAX
	};

	const Sprite* TimelineTexture::getScoreStatsSprite(const InsertMode& mode)
	{
		if (!toolbarTex)
			return nullptr;
		int modeIdx = static_cast<int>(mode);
		if (!isArrayIndexInBounds(modeIdx, insertModes))
			return nullptr;
		switch (mode)
		{
		case InsertMode::InsertLong:
			return sprites[INS_LONG_SPR_];
		case InsertMode::InsertFlick:
			return sprites[INS_FLICK_SPR_ + int(FlickType::Default)];
		case InsertMode::InsertLongMid:
			return sprites[INS_LONG_MID_SPR_];
		case InsertMode::InsertGuide:
			return sprites[INS_GUIDE_SPR_ + int(GuideColor::Green)];
		default:
			return sprites[modeIdx];
		}
	}

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
			return sprites[INS_LONG_SPR_ + int(edit.easeType)];
		case InsertMode::InsertFlick:
			return sprites[INS_FLICK_SPR_ + int(edit.flickType)];
		case InsertMode::InsertLongMid:
			return sprites[INS_LONG_MID_SPR_ + int(edit.stepType)];
		case InsertMode::InsertGuide:
			return sprites[INS_GUIDE_SPR_ + int(edit.colorType) +
			               int(edit.fadeType) * std::size(guideColors)];
		case InsertMode::InsertHiSpeed:
			if (edit.hiSpeedHideNotes)
				return sprites[INS_HISPEED_SPR_HIDE];
			[[fallthrough]];
		default:
			return sprites[modeIdx];
		}
		return toolbarTex->getSprite(name);
	}

	bool TimelineTexture::load()
	{
		auto path =
		    Application::getInstance().getResourcePath("textures", "editor", "timeline_tools.png");
		std::string filename = IO::toString(path);
		if (!fs::exists(path))
		{
			fprintf(stderr, "ERROR: Could not find texture file %s\n", filename.c_str());
			return false;
		}
		toolbarTex = std::make_unique<Texture>(filename);
		sprites.clear();
		sprites.resize(INS_SPR_MAX, nullptr);
		for (int i = 0; i < std::size(insertModes); i++)
			sprites[INS_SPR_SELECT + i] = toolbarTex->getSprite(insertModes[i]);
		for (int i = 0; i < std::size(easeTypes); i++)
			sprites[INS_LONG_SPR_ + i] =
			    toolbarTex->getSprite(IO::formatString("timeline_hold_%s", easeTypes[i]));
		for (int i = 0; i < std::size(stepTypes); i++)
			sprites[INS_LONG_MID_SPR_ + i] =
			    toolbarTex->getSprite(IO::formatString("timeline_hold_step_%s", stepTypes[i]));
		for (int i = 0; i < std::size(flickTypes); i++)
			sprites[INS_FLICK_SPR_ + i] =
			    toolbarTex->getSprite(IO::formatString("timeline_flick_%s", flickTypes[i]));
		for (int i = 0; i < std::size(guideColors); i++)
			for (int j = 0; j < std::size(fadeTypes); j++)
				sprites[INS_GUIDE_SPR_ + i + j * std::size(guideColors)] = toolbarTex->getSprite(
				    IO::formatString("timeline_guide_%s_%s", guideColors[i], fadeTypes[j]));
		sprites[INS_HISPEED_SPR_HIDE] = toolbarTex->getSprite("timeline_hi_speed_hide");
		return true;
	}

	const Texture* TimelineTexture::getToolbarTexture() const { return toolbarTex.get(); }

	enum SPR_
	{
		// notes_xx
		SPR_NOTE_CRITICAL,
		SPR_NOTE_LONG,
		SPR_NOTE_TAP,
		SPR_NOTE_FLICK,
		SPR_NOTE_FRICTION,
		SPR_NOTE_FRICTION_CRITICAL,
		SPR_NOTE_FRICTION_FLICK,
		// notes_long_among_xx
		SPR_NOTE_LONG_AMONG,
		SPR_NOTE_LONG_AMONG_CRITICAL,
		// notes_friction_among_
		SPR_NOTE_FRICTION_AMONG,
		SPR_NOTE_FRICTION_AMONG_CRITICAL,
		SPR_NOTE_FRICTION_AMONG_FLICK,
		// notes_flick_arrow_
		SPR_FLICK_ARROW_01,
		SPR_FLICK_ARROW_01_DIAGONAL = SPR_FLICK_ARROW_01 + 6,
		// notes_flick_arrow_crtcl_
		SPR_FLICK_ARROW_CRITICAL_01 = SPR_FLICK_ARROW_01_DIAGONAL + 6,
		SPR_FLICK_ARROW_CRITICAL_01_DIAGONAL = SPR_FLICK_ARROW_CRITICAL_01 + 6,
		// long_xx
		SPR_LONG_NORMAL = SPR_FLICK_ARROW_CRITICAL_01_DIAGONAL + 6,
		SPR_LONG_ACTIVE,
		SPR_LONG_CRITICAL_NORMAL,
		SPR_LONG_CRITICAL_ACTIVE,
		// guide_color_xx
		SPR_GUIDE_COLOR_00,
		SPR_GUIDE_COLOR_01,
		SPR_GUIDE_COLOR_02,
		SPR_GUIDE_COLOR_03,
		SPR_GUIDE_COLOR_04,
		SPR_GUIDE_COLOR_05,
		SPR_GUIDE_COLOR_06,
		SPR_GUIDE_COLOR_07,
		// simultaneousLine
		SPR_SIMULTANEOUS_CONNECTION,
		// cc_notes_xx
		SPR_NOTE_DAMAGE,
		// ns_dummy_cross
		SPR_DUMMY_CROSS,

		SPR_MAX
	};

	const Sprite* NoteTexture::getNoteSprite(const Note& note) const
	{
		if (!texture)
			return nullptr;
		SPR_ spr;
		switch (note.type)
		{
		case NoteType::Tap:
			if (!note.isTrace())
				spr = note.isCrit()                            ? SPR_NOTE_CRITICAL
				      : note.isFlick()                         ? SPR_NOTE_FLICK
				      : hasFlag(note.flag, NoteFlag::LongNote) ? SPR_NOTE_LONG
				                                               : SPR_NOTE_TAP;
			else
				spr = note.isCrit()    ? SPR_NOTE_FRICTION_CRITICAL
				      : note.isFlick() ? SPR_NOTE_FRICTION_FLICK
				                       : SPR_NOTE_FRICTION;
			break;
		case NoteType::Tick:
			spr = note.isCrit() ? SPR_NOTE_LONG_AMONG_CRITICAL : SPR_NOTE_LONG_AMONG;
			break;
		case NoteType::Damage:
			spr = SPR_NOTE_DAMAGE;
			break;
		default:
			return nullptr;
		}

		return sprites[spr];
	}

	const Sprite* NoteTexture::getFlickArrowSprite(const Note& note) const
	{
		if (!texture)
			return nullptr;
		int offset = note.isCrit() ? 12 : 0;
		bool isDiagonal = note.flick == FlickType::Left || note.flick == FlickType::Right ||
		                  note.flick == FlickType::DownLeft || note.flick == FlickType::DownRight;
		offset += isDiagonal ? 6 : 0;
		int arrowSize = std::min(std::max(int(note.width), 1), 6) - 1;
		return sprites[SPR_FLICK_ARROW_01 + offset + arrowSize];
	}

	const Sprite* NoteTexture::getFrictionSprite(const Note& note) const
	{
		return sprites[note.isCrit()    ? SPR_NOTE_FRICTION_AMONG_CRITICAL
		               : note.isFlick() ? SPR_NOTE_FRICTION_AMONG_FLICK
		                                : SPR_NOTE_FRICTION_AMONG];
	}

	const Sprite* NoteTexture::getDummyCrossSprite() const
	{
		return texture ? sprites[SPR_DUMMY_CROSS] : nullptr;
	}

	const Sprite* NoteTexture::getHoldNoteSprite(const HoldNoteStep& holdStep) const
	{
		if (!texture)
			return nullptr;
		if (holdStep.isGuide())
			return sprites[SPR_GUIDE_COLOR_00 + static_cast<int>(holdStep.guideColor)];
		return sprites[!holdStep.isCrit() ? SPR_LONG_NORMAL : SPR_LONG_CRITICAL_NORMAL];
	}

	bool NoteTexture::load(const std::string& profile)
	{
		auto path =
		    Application::getInstance().getResourcePath("textures", "note", profile, "notes.png");
		auto filename = IO::toString(path);
		if (!fs::exists(path))
		{
			fprintf(stderr, "ERROR: Could not find texture file %s\n", filename.c_str());
			return false;
		}
		texture = std::make_unique<Texture>(filename);

		sprites.clear();
		sprites.resize(SPR_MAX, nullptr);
		for (int i = SPR_NOTE_CRITICAL; i <= SPR_NOTE_FRICTION_FLICK; i++)
			sprites[i] = texture->getSprite(IO::formatString("notes_%d", i));
		constexpr const char* noteNames[] = { "long_among", "long_among_crtcl",
			                                  "friction_among_long", "friction_among_crtcl",
			                                  "friction_among_flick" };
		for (int i = 0; i < std::size(noteNames); i++)
			sprites[i + SPR_NOTE_LONG_AMONG] =
			    texture->getSprite(IO::formatString("notes_%s", noteNames[i]));
		for (int i = 0; i < 6; ++i)
		{
			sprites[SPR_FLICK_ARROW_01 + i] =
			    texture->getSprite(IO::formatString("notes_flick_arrow_%02d", i + 1));
			sprites[SPR_FLICK_ARROW_01_DIAGONAL + i] =
			    texture->getSprite(IO::formatString("notes_flick_arrow_%02d_diagonal", i + 1));
			sprites[SPR_FLICK_ARROW_CRITICAL_01 + i] =
			    texture->getSprite(IO::formatString("notes_flick_arrow_crtcl_%02d", i + 1));
			sprites[SPR_FLICK_ARROW_CRITICAL_01_DIAGONAL + i] = texture->getSprite(
			    IO::formatString("notes_flick_arrow_crtcl_%02d_diagonal", i + 1));
		}
		sprites[SPR_LONG_NORMAL] = texture->getSprite("long_normal");
		sprites[SPR_LONG_ACTIVE] = texture->getSprite("long_active");
		sprites[SPR_LONG_CRITICAL_NORMAL] = texture->getSprite("long_critical_normal");
		sprites[SPR_LONG_CRITICAL_ACTIVE] = texture->getSprite("long_critical_active");
		for (int i = 0; i < 8; i++)
			sprites[SPR_GUIDE_COLOR_00 + i] =
			    texture->getSprite(IO::formatString("guide_color_%d", i));
		sprites[SPR_SIMULTANEOUS_CONNECTION] = texture->getSprite("simultaneousLine");
		sprites[SPR_NOTE_DAMAGE] = texture->getSprite("cc_notes_0");
		sprites[SPR_DUMMY_CROSS] = texture->getSprite("ns_dummy_cross");

		noteMargin = texture->getConfig("noteMargin");
		noteSideWidth = texture->getConfig("noteSideWidth", 120);
		holdMargin = texture->getConfig("holdMargin");
		holdSideWidth = texture->getConfig("holdSideWidth", 43);
		return true;
	}

	const Texture* NoteTexture::getTexture() const { return texture.get(); }

	const std::vector<std::string>& NoteTexture::getProfiles() const { return profiles; }

	void NoteTexture::scanProfiles()
	{
		profiles.clear();
		auto notePath = Application::getInstance().getResourcePath("textures", "note");
		if (!fs::exists(notePath))
			return;
		for (auto&& entry : fs::directory_iterator(notePath))
			if (entry.is_directory())
				profiles.push_back(entry.path().filename().string());
	}

	NoteMapping NoteTexture::getTapNoteMapping(const Note& note) const
	{
		const Sprite* sprite = getNoteSprite(note);
		if (!sprite)
			return {};
		const float left = sprite->getX1() + noteMargin, center = left + noteSideWidth,
		            right = sprite->getX2() - noteMargin - noteSideWidth;
		const float w = texture->getWidth(), h = texture->getHeight();
		return { left / w, center / w, right / w, sprite->getY1() / h, sprite->getY2() / h };
	}

	NoteMapping NoteTexture::getHoldStepMapping(const HoldNoteStep& holdStep) const
	{
		const Sprite* sprite = getHoldNoteSprite(holdStep);
		if (!sprite)
			return {};
		const float left = sprite->getX1() + holdMargin, center = left + holdSideWidth,
		            right = sprite->getX2() - holdMargin - holdSideWidth;
		const float w = texture->getWidth(), h = texture->getHeight();
		return { left / w, center / w, right / w, sprite->getY1() / h, sprite->getY2() / h };
	}
}
