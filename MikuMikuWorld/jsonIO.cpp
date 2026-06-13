#include "JsonIO.h"

using namespace nlohmann;

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
