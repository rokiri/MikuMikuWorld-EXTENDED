#include "JsonIO.h"
#include "Math.h"
#include "ApplicationConfiguration.h"
#include "InputBinding.h"

using namespace nlohmann;
using namespace jsonIO;

namespace jsonIO
{
	mmw::Note jsonToNote(const json& data, mmw::NoteType type)
	{
		mmw::Note note(type);

		note.tick = tryGetValue<int>(data, "tick", 0);
		note.lane = tryGetValue<float>(data, "lane", 0);
		note.width = tryGetValue<float>(data, "width", 3);

		if (note.getType() != mmw::NoteType::HoldMid)
		{
			note.critical = tryGetValue<bool>(data, "critical", false);
			note.friction = tryGetValue<bool>(data, "friction", false);
		}

		if (!note.hasEase())
		{
			std::string flickString = tryGetValue<std::string>(data, "flick", mmw::flickTypes[0]);
			std::transform(flickString.begin(), flickString.end(), flickString.begin(), ::tolower);
			if (flickString == "up")
				flickString = mmw::flickTypes[(int)mmw::FlickType::Default];

			for (size_t i = 0; i < std::size(mmw::flickTypes); i++)
			{
				if (flickString == mmw::flickTypes[i])
					note.flick = static_cast<mmw::FlickType>(i);
			}
		}

		note.dummy = tryGetValue<bool>(data, "dummy", false);

		return note;
	}

	static json noteToJson(const mmw::Note& note)
	{
		json data;
		data["tick"] = note.tick;
		data["lane"] = note.lane;
		data["width"] = note.width;

		if (note.getType() != mmw::NoteType::HoldMid)
		{
			data["critical"] = note.critical;
			data["friction"] = note.friction;
		}

		if (!note.hasEase())
		{
			data["flick"] = mmw::flickTypes[(int)note.flick];
		}

		data["dummy"] = note.dummy;

		return data;
	}

	json noteSelectionToJson(const mmw::Score& score,
	                         const std::unordered_set<mmw::id_t>& selection,
	                         const std::unordered_set<mmw::id_t>& hiSpeedSelection, int baseTick)
	{
		json retData, notes, holds, damages, hiSpeedChanges;
		std::unordered_set<mmw::id_t> selectedNotes;
		std::unordered_set<mmw::id_t> selectedHolds;
		std::unordered_set<mmw::id_t> selectedDamages;

		for (mmw::id_t id : selection)
		{
			if (score.notes.find(id) == score.notes.end())
				continue;

			const mmw::Note& note = score.notes.at(id);
			switch (note.getType())
			{
			case mmw::NoteType::Tap:
				selectedNotes.insert(note.ID);
				break;
			case mmw::NoteType::Hold:
				selectedHolds.insert(note.ID);
				break;

			case mmw::NoteType::HoldMid:
			case mmw::NoteType::HoldEnd:
				selectedHolds.insert(note.parentID);
				break;

			case mmw::NoteType::Damage:
				selectedDamages.insert(note.ID);
				break;
			default:
				break;
			}
		}

		for (mmw::id_t id : selectedNotes)
		{
			const mmw::Note& note = score.notes.at(id);
			json data = noteToJson(note);
			data["tick"] = note.tick - baseTick;

			notes.push_back(data);
		}
		for (mmw::id_t id : selectedDamages)
		{
			const mmw::Note& note = score.notes.at(id);
			json data = noteToJson(note);
			data["tick"] = note.tick - baseTick;

			damages.push_back(data);
		}
		for (int id : hiSpeedSelection)
		{
			const mmw::HiSpeedChange& hispeed = score.hiSpeedChanges.at(id);
			json data; 
			data["tick"] = hispeed.tick - baseTick;
			data["speed"] = hispeed.speed;
			data["skip"] = hispeed.skips;
			data["ease"] = hispeed.ease;
			data["hideNotes"] = hispeed.hideNotes;

			hiSpeedChanges.push_back(data);
		}

		for (mmw::id_t id : selectedHolds)
		{
			const mmw::HoldNote& hold = score.holdNotes.at(id);
			const mmw::Note& start = score.notes.at(hold.start.ID);
			const mmw::Note& end = score.notes.at(hold.end);

			json holdData, stepsArray;

			json holdStart = noteToJson(start);
			holdStart["tick"] = start.tick - baseTick;
			holdStart["ease"] = mmw::easeTypes[(int)hold.start.ease];
			holdStart["type"] = mmw::holdTypes[(int)hold.startType];

			for (auto& step : hold.steps)
			{
				const mmw::Note& mid = score.notes.at(step.ID);
				json stepData = noteToJson(mid);
				stepData["tick"] = mid.tick - baseTick;
				stepData["type"] = mmw::stepTypes[(int)step.type];
				stepData["ease"] = mmw::easeTypes[(int)step.ease];

				stepsArray.push_back(stepData);
			}

			json holdEnd = noteToJson(end);
			holdEnd["tick"] = end.tick - baseTick;
			holdEnd["type"] = mmw::holdTypes[(int)hold.endType];

			holdData["start"] = holdStart;
			holdData["steps"] = stepsArray;
			holdData["end"] = holdEnd;
			holdData["fade"] = mmw::fadeTypes[(int)hold.fadeType];
			holdData["guide"] = mmw::guideColors[(int)hold.guideColor];
			holdData["dummy"] = hold.dummy;
			holds.push_back(holdData);
		}

		retData["notes"] = notes;
		retData["holds"] = holds;
		retData["damages"] = damages;
		retData["hiSpeedChanges"] = hiSpeedChanges;
		return retData;
	}
}

namespace std::chrono
{
	void to_json(json& j, const system_clock::time_point& tp)
	{
		j = duration_cast<milliseconds>(tp.time_since_epoch()).count();
	}

	void from_json(const json& j, system_clock::time_point& tp)
	{
		milliseconds::rep t = j;
		tp = system_clock::time_point(milliseconds(t));
	}
}

namespace MikuMikuWorld
{
	void to_json(json& j_vec, const Vector2& vec)
	{
		j_vec["x"] = vec.x;
		j_vec["y"] = vec.y;
	}

	void from_json(const json& j_vec, Vector2& vec)
	{
		j_vec.at("x").get_to(vec.x);
		j_vec.at("y").get_to(vec.x);
	}

	void to_json(nlohmann::json& j_color, const Color& color)
	{
		j_color["r"] = color.r;
		j_color["g"] = color.g;
		j_color["b"] = color.b;
		j_color["a"] = color.a;
	}

	void from_json(const nlohmann::json& j_color, Color& color)
	{
		j_color.at("r").get_to(color.r);
		j_color.at("g").get_to(color.g);
		j_color.at("b").get_to(color.b);
		j_color.at("a").get_to(color.a);
	}

	void to_json(nlohmann::json& j_input, const InputConfiguration& input)
	{
		for (const auto& pBinding : allInputBindings)
		{
			auto& binding = (input.*pBinding);
			auto& j_binding = j_input[binding.name] = json::array();
			for (size_t i = 0; i < binding.count; ++i)
				if (binding.bindings[i].keyCode != ImGuiKey_None)
					j_binding.push_back(ToSerializedString(binding.bindings[i]));
		}
	}

	void from_json(const nlohmann::json& j, InputConfiguration& input)
	{
		if (!j.is_object())
			return;
		for (auto& [key, value] : j.items())
		{
			if (!value.is_array() || !value.empty())
				continue;
			for (const auto& pBinding : allInputBindings)
			{
				auto& multBinding = (input.*pBinding);
				if (multBinding.name == key)
				{
					size_t maxKeys = std::min(value.size(), multBinding.bindings.size());
					for (size_t i = 0; i < maxKeys; i++)
						multBinding.bindings[i] = FromSerializedString(value[i]);
					break;
				}
			}
		}
	}

	void to_json(json& j_cfg, const ApplicationConfiguration& cfg)
	{
		j_cfg["version"] = cfg.version;
		j_cfg["language"] = cfg.language;
		j_cfg["debug"] = cfg.debugEnabled;
		j_cfg["recent_files"] = cfg.recentFiles;

		{
			auto& j_file = j_cfg["file"];
			j_file["minify_output"] = cfg.minifyOutput;
			j_file["export_format"] = cfg.defaultExportFormat;
		}
		{
			auto& j_window = j_cfg["window"];
			j_window["position"] = cfg.windowPos;
			j_window["size"] = cfg.windowSize;
			j_window["maximized"] = cfg.maximized;
			j_window["vsync"] = cfg.vsync;
			j_window["show_fps"] = cfg.showFPS;
		}
		{
			auto& j_timeline = j_cfg["timeline"];
			j_timeline["lane_width"] = cfg.timelineWidth;
			j_timeline["notes_height"] = cfg.notesHeight;
			j_timeline["notes_skin"] = cfg.notesSkin;
			j_timeline["match_notes_size_to_timeline"] = cfg.matchNotesSizeToTimeline;
			j_timeline["division"] = cfg.division;
			j_timeline["zoom"] = cfg.zoom;
			j_timeline["lane_opacity"] = cfg.laneOpacity;
			j_timeline["background_brightness"] = cfg.backgroundBrightness;
			j_timeline["draw_background"] = cfg.drawBackground;
			j_timeline["background_image"] = cfg.backgroundImage;
			j_timeline["smooth_scrolling_enable"] = cfg.useSmoothScrolling;
			j_timeline["smooth_scrolling_time"] = cfg.smoothScrollingTime;
			j_timeline["scroll_speed_normal"] = cfg.scrollSpeedNormal;
			j_timeline["scroll_speed_fast"] = cfg.scrollSpeedShift;
			j_timeline["draw_waveform"] = cfg.drawWaveform;
			j_timeline["return_to_last_tick_on_pause"] = cfg.returnToLastSelectedTickOnPause;
			j_timeline["cursor_position_threshold"] = cfg.cursorPositionThreshold;
			j_timeline["show_tick_in_properties"] = cfg.showTickInProperties;
		}
		{
			auto& j_theme = j_cfg["theme"];
			j_theme["accent_color"] = cfg.accentColor;
			j_theme["user_color"] = cfg.userColor;
			j_theme["base_theme"] = static_cast<int>(cfg.baseTheme);
		}
		{
			auto& j_save = j_cfg["save"];
			j_save["auto_save_enabled"] = cfg.autoSaveEnabled;
			j_save["auto_save_interval"] = cfg.autoSaveInterval;
			j_save["auto_save_max_count"] = cfg.autoSaveMaxCount;
		}
		{
			auto& j_audio = j_cfg["audio"];
			j_audio["se_profile"] = cfg.seProfileIndex;
			j_audio["master_volume"] = cfg.masterVolume;
			j_audio["bgm_volume"] = cfg.bgmVolume;
			j_audio["se_volume"] = cfg.seVolume;
		}
		{
			auto& j_update = j_cfg["update"];
			j_update["lastUpdate"] = cfg.lastUpdateCheck;
			j_update["lastVersion"] = cfg.latestFetchAppVersion;
		}
		{
			auto& j_input = j_cfg["config"];
			j_input["bindings"] = cfg.input;
		}
	}

	void from_json(const json& j_cfg, ApplicationConfiguration& cfg)
	{
		optional_get_to(j_cfg, "version", cfg.version);
		optional_get_to(j_cfg, "language", cfg.language);
		optional_get_to(j_cfg, "debug", cfg.debugEnabled);
		optional_get_to(j_cfg, "recent_files", cfg.recentFiles);
		if (cfg.recentFiles.size() > maxRecentFilesEntries)
			cfg.recentFiles.resize(maxRecentFilesEntries);
		if (keyExists(j_cfg, "file"))
		{
			auto& j_file = j_cfg["file"];
			optional_get_to(j_file, "minify_output", cfg.minifyOutput);
			optional_get_to(j_file, "export_format", cfg.defaultExportFormat);
		}
		if (keyExists(j_cfg, "window"))
		{
			const json& j_window = j_cfg["window"];
			optional_get_to(j_window, "maximized", cfg.maximized);
			optional_get_to(j_window, "vsync", cfg.vsync);
			optional_get_to(j_window, "show_fps", cfg.showFPS);
			optional_get_to(j_window, "position", cfg.windowPos);
			optional_get_to(j_window, "size", cfg.windowSize);
		}
		if (jsonIO::keyExists(j_cfg, "timeline"))
		{
			const json& j_timeline = j_cfg["timeline"];
			optional_get_to(j_timeline, "lane_width", cfg.timelineWidth);
			optional_get_to(j_timeline, "notes_height", cfg.notesHeight);
			optional_get_to(j_timeline, "notes_skin", cfg.notesSkin);
			optional_get_to(j_timeline, "match_notes_size_to_timeline",
			                cfg.matchNotesSizeToTimeline);
			optional_get_to(j_timeline, "division", cfg.division);
			optional_get_to(j_timeline, "zoom", cfg.zoom);
			optional_get_to(j_timeline, "lane_opacity", cfg.laneOpacity);
			optional_get_to(j_timeline, "background_brightness", cfg.backgroundBrightness);
			optional_get_to(j_timeline, "draw_background", cfg.drawBackground);
			optional_get_to(j_timeline, "background_image", cfg.backgroundImage);
			optional_get_to(j_timeline, "smooth_scrolling_enable", cfg.useSmoothScrolling);
			optional_get_to(j_timeline, "smooth_scrolling_time", cfg.smoothScrollingTime);
			optional_get_to(j_timeline, "scroll_speed_normal", cfg.scrollSpeedNormal);
			optional_get_to(j_timeline, "scroll_speed_fast", cfg.scrollSpeedShift);
			optional_get_to(j_timeline, "draw_waveform", cfg.drawWaveform);
			optional_get_to(j_timeline, "return_to_last_tick_on_pause",
			                cfg.returnToLastSelectedTickOnPause);
			optional_get_to(j_timeline, "cursor_position_threshold", cfg.cursorPositionThreshold);
			optional_get_to(j_timeline, "show_tick_in_properties", cfg.showTickInProperties);
		}

		if (jsonIO::keyExists(j_cfg, "theme"))
		{
			const json& j_theme = j_cfg["theme"];
			optional_get_to(j_theme, "accent_color", cfg.accentColor);
			optional_get_to(j_theme, "user_color", cfg.userColor);

			int baseTheme = static_cast<int>(cfg.baseTheme);
			optional_get_to(j_theme, "base_theme", baseTheme);
			cfg.baseTheme = static_cast<BaseTheme>(baseTheme);
		}

		if (jsonIO::keyExists(j_cfg, "save"))
		{
			const json& j_save = j_cfg["save"];
			optional_get_to(j_save, "auto_save_enabled", cfg.autoSaveEnabled);
			optional_get_to(j_save, "auto_save_interval", cfg.autoSaveInterval);
			optional_get_to(j_save, "auto_save_max_count", cfg.autoSaveMaxCount);
		}

		if (jsonIO::keyExists(j_cfg, "audio"))
		{
			const json& j_audio = j_cfg["audio"];
			optional_get_to(j_audio, "se_profile", cfg.seProfileIndex);
			optional_get_to(j_audio, "master_volume", cfg.masterVolume);
			optional_get_to(j_audio, "bgm_volume", cfg.bgmVolume);
			optional_get_to(j_audio, "se_volume", cfg.seVolume);
		}

		if (jsonIO::keyExists(j_cfg, "update"))
		{
			const json& j_update = j_cfg["update"];
			optional_get_to(j_update, "lastUpdate", cfg.lastUpdateCheck);
			optional_get_to(j_update, "lastVersion", cfg.latestFetchAppVersion);
		}

		if (jsonIO::keyExists(j_cfg, "config"))
		{
			const json& j_input = j_cfg["config"];
			optional_get_to(j_input, "bindings", cfg.input);
		}
	}
}