#include "SonolusSerializer.h"
#include "Constants.h"
#include "IO.h"
#include "File.h"

#ifdef _DEBUG
#define PRINT_DEBUG(...)                                                                           \
	do                                                                                             \
	{                                                                                              \
		fprintf(stderr, __VA_ARGS__);                                                              \
		fprintf(stderr, "\n");                                                                     \
	} while (0)
#else
#define PRINT_DEBUG(...)                                                                           \
	do                                                                                             \
	{                                                                                              \
	} while (0)
#endif

using json = nlohmann::json;
using namespace Sonolus;

namespace MikuMikuWorld
{
	void SonolusSerializer::serialize(const Score& score, std::string filename)
	{
		LevelData levelData = engine->serialize(score);
		std::string serializedData = json(levelData).dump(prettyDump ? 2 : -1);
		std::vector<uint8_t> serializedBytes(serializedData.begin(), serializedData.end());
		if (compressData)
			serializedBytes = IO::deflateGzip(serializedBytes);

		IO::File levelFile(filename, IO::FileMode::WriteBinary);
		levelFile.writeAllBytes(serializedBytes);
		levelFile.flush();
		levelFile.close();
	}

	Score SonolusSerializer::deserialize(std::string filename)
	{
		if (!IO::File::exists(filename))
			return {};
		IO::File levelFile(filename, IO::FileMode::ReadBinary);
		std::vector<uint8_t> bytes = levelFile.readAllBytes();
		levelFile.close();
		if (IO::isGzipCompressed(bytes))
			bytes = IO::inflateGzip(bytes);
		json levelDataJson = json::parse(std::string(bytes.begin(), bytes.end()));
		LevelData levelData;
		levelDataJson.get_to(levelData);

		return engine->deserialize(levelData);
	}

	double SonolusEngine::toBgmOffset(float musicOffset)
	{
		return musicOffset == 0 ? 0.0 : -static_cast<double>(musicOffset) / 1000.0;
	}

	LevelDataEntity SonolusEngine::toBpmChangeEntity(const Tempo& tempo)
	{
		return { "#BPM_CHANGE",
			     { { "#BEAT", ticksToBeats(tempo.tick) }, { "#BPM", static_cast<RealType>(tempo.bpm) } } };
	}

	SonolusEngine::RealType SonolusEngine::ticksToBeats(TickType ticks, TickType beatTicks)
	{
		return static_cast<RealType>(ticks) / beatTicks;
	}

	SonolusEngine::RealType SonolusEngine::widthToSize(WidthType width)
	{
		return static_cast<RealType>(width) / 2;
	}

	SonolusEngine::RealType SonolusEngine::toSonolusLane(LaneType lane, WidthType width)
	{
		return (lane - 6) + (static_cast<RealType>(width) / 2);
	}

	float SonolusEngine::fromBgmOffset(double bgmOffset)
	{
		return bgmOffset == 0 ? 0.f : static_cast<float>(-bgmOffset * 1000.0);
	}

	bool SonolusEngine::fromBpmChangeEntity(const Sonolus::LevelDataEntity& bpmChangeEntity,
	                                        Tempo& tempo)
	{
		float beat;
		if (!bpmChangeEntity.tryGetDataValue("#BEAT", beat))
		{
			PRINT_DEBUG("Missing #BEAT key on #BPM_CHANGE");
			return false;
		}
		tempo.tick = beatsToTicks(beat);
		float bpm;
		if (!bpmChangeEntity.tryGetDataValue("#BPM", bpm))
		{
			PRINT_DEBUG("Missing #BPM key on #BPM_CHANGE");
			return false;
		}
		tempo.bpm = bpm;
		return true;
	}

	SonolusEngine::TickType SonolusEngine::beatsToTicks(RealType beats, TickType beatTicks)
	{
		return static_cast<TickType>(std::lround(beats * beatTicks));
	}

	SonolusEngine::WidthType SonolusEngine::sizeToWidth(RealType size) { return size * 2; }

	SonolusEngine::LaneType SonolusEngine::toNativeLane(RealType lane, RealType size)
	{
		return lane - size + 6;
	}

	Sonolus::LevelData PySekaiEngine::serialize(const Score& score)
	{
		LevelData levelData;
		levelData.bgmOffset = toBgmOffset(score.metadata.musicOffset);

		levelData.entities.emplace_back("#BPM_CHANGE",
		    LevelDataEntity::MapDataType{ { "#BEAT", RealType(0) }, { "#BPM", RealType(120) } });

		for (const auto& tempo : score.tempoChanges)
			levelData.entities.emplace_back(toBpmChangeEntity(tempo));

		std::unordered_map<int, size_t> layerGroupIndex;
		for (int i = 0; i < (int)score.layers.size(); ++i)
		{
			layerGroupIndex[i] = levelData.entities.size();
			levelData.entities.emplace_back(toGroupEntity(score.layers[i]));
		}

		std::unordered_map<int, size_t> lastHiSpeedIndex;
		for (int i = 0; i < (int)score.layers.size(); ++i)
			lastHiSpeedIndex[i] = layerGroupIndex[i];

		for (const auto& [_, hs] : score.hiSpeedChanges)
		{
			int layer = hs.layer;
			size_t groupIdx = layerGroupIndex.count(layer) ? layerGroupIndex[layer] : layerGroupIndex[0];
			size_t prevIdx = lastHiSpeedIndex.count(layer) ? lastHiSpeedIndex[layer] : groupIdx;

			size_t newIdx = levelData.entities.size();
			std::string groupName;
			if (!levelData.entities[groupIdx].name.empty())
				groupName = levelData.entities[groupIdx].name;
			else
			{
				levelData.entities[groupIdx].name = IO::formatString("g%d", layer);
				groupName = levelData.entities[groupIdx].name;
			}

			levelData.entities.emplace_back(toTimeScaleEntity(hs, groupName));
			std::string newName = IO::formatString("hs%d_%d", layer, (int)newIdx);
			levelData.entities[newIdx].name = newName;

			if (prevIdx == groupIdx)
				levelData.entities[prevIdx].data["first"] = newName;
			else
				levelData.entities[prevIdx].data["next"] = newName;

			lastHiSpeedIndex[layer] = newIdx;
		}

		for (const auto& [_, skill] : score.skills)
			levelData.entities.emplace_back(toSkillEntity(skill));

		if (score.fever.startTick >= 0 && score.fever.endTick >= score.fever.startTick)
		{
			auto [feverChance, feverStart] = toFeverEntities(score.fever);
			levelData.entities.emplace_back(feverChance);
			levelData.entities.emplace_back(feverStart);
		}

		std::multimap<int, size_t> simBuilder;

		for (const auto& [id, note] : score.notes)
		{
			if (note.isHold())
				continue;
			int layer = note.layer;
			size_t groupIdx = layerGroupIndex.count(layer) ? layerGroupIndex[layer] : layerGroupIndex[0];
			std::string groupName;
			if (!levelData.entities[groupIdx].name.empty())
				groupName = levelData.entities[groupIdx].name;
			else
			{
				levelData.entities[groupIdx].name = IO::formatString("g%d", layer);
				groupName = levelData.entities[groupIdx].name;
			}
			size_t entIdx = levelData.entities.size();
			levelData.entities.emplace_back(toNoteEntity(note, getSingleNoteArchetype(note), groupName));
			simBuilder.emplace(note.tick, entIdx);
		}

		for (const auto& [holdID, hold] : score.holdNotes)
		{
			const Note& startNote = score.notes.at(hold.start.ID);
			int layer = startNote.layer;
			size_t groupIdx = layerGroupIndex.count(layer) ? layerGroupIndex[layer] : layerGroupIndex[0];
			std::string groupName;
			if (!levelData.entities[groupIdx].name.empty())
				groupName = levelData.entities[groupIdx].name;
			else
			{
				levelData.entities[groupIdx].name = IO::formatString("g%d", layer);
				groupName = levelData.entities[groupIdx].name;
			}

			size_t startIdx = levelData.entities.size();
			levelData.entities.emplace_back(
			    toNoteEntity(startNote, getHoldNoteArchetype(startNote, true, false), groupName, &hold, &hold.start));
			std::string startName = IO::formatString("n%d", (int)startIdx);
			levelData.entities[startIdx].name = startName;
			simBuilder.emplace(startNote.tick, startIdx);

			size_t prevIdx = startIdx;
			for (int i = 0; i < (int)hold.steps.size(); ++i)
			{
				const HoldStep& step = hold.steps[i];
				const Note& midNote = score.notes.at(step.ID);
				int midLayer = midNote.layer;
				size_t midGroupIdx = layerGroupIndex.count(midLayer) ? layerGroupIndex[midLayer] : layerGroupIndex[0];
				std::string midGroupName;
				if (!levelData.entities[midGroupIdx].name.empty())
					midGroupName = levelData.entities[midGroupIdx].name;
				else
				{
					levelData.entities[midGroupIdx].name = IO::formatString("g%d", midLayer);
					midGroupName = levelData.entities[midGroupIdx].name;
				}

				size_t midIdx = levelData.entities.size();
				levelData.entities.emplace_back(
				    toNoteEntity(midNote, getHoldNoteArchetype(midNote, false, false), midGroupName, &hold, &step));
				std::string midName = IO::formatString("n%d", (int)midIdx);
				levelData.entities[midIdx].name = midName;
				levelData.entities[prevIdx].data["next"] = midName;
				simBuilder.emplace(midNote.tick, midIdx);
				prevIdx = midIdx;
			}

			const Note& endNote = score.notes.at(hold.end);
			int endLayer = endNote.layer;
			size_t endGroupIdx = layerGroupIndex.count(endLayer) ? layerGroupIndex[endLayer] : layerGroupIndex[0];
			std::string endGroupName;
			if (!levelData.entities[endGroupIdx].name.empty())
				endGroupName = levelData.entities[endGroupIdx].name;
			else
			{
				levelData.entities[endGroupIdx].name = IO::formatString("g%d", endLayer);
				endGroupName = levelData.entities[endGroupIdx].name;
			}

			size_t endIdx = levelData.entities.size();
			levelData.entities.emplace_back(
			    toNoteEntity(endNote, getHoldNoteArchetype(endNote, false, true), endGroupName, &hold, nullptr));
			std::string endName = IO::formatString("n%d", (int)endIdx);
			levelData.entities[endIdx].name = endName;
			levelData.entities[prevIdx].data["next"] = endName;
			simBuilder.emplace(endNote.tick, endIdx);
		}

		std::vector<size_t> simEntities;
		for (auto it = simBuilder.begin(), end = simBuilder.end(); it != end;)
		{
			auto [rangeBegin, rangeEnd] = simBuilder.equal_range(it->first);
			simEntities.clear();
			for (auto jt = rangeBegin; jt != rangeEnd; ++jt)
				simEntities.push_back(jt->second);
			std::sort(simEntities.begin(), simEntities.end(),
			    [&](size_t a, size_t b)
			    {
				    return levelData.entities[a].getDataValue<RealType>("lane") <
				           levelData.entities[b].getDataValue<RealType>("lane");
			    });
			for (size_t i = 1; i < simEntities.size(); ++i)
			{
				levelData.entities.push_back({ "SimLine",
				    { { "left", levelData.entities[simEntities[i - 1]].name },
				      { "right", levelData.entities[simEntities[i]].name } } });
			}
			it = rangeEnd;
		}

		return levelData;
	}

	Score PySekaiEngine::deserialize(const Sonolus::LevelData& levelData)
	{
		Score score;
		score.metadata.musicOffset = fromBgmOffset(levelData.bgmOffset);

		std::unordered_map<RefType, size_t> entityNameMap;
		for (size_t i = 0; i < levelData.entities.size(); ++i)
		{
			const auto& e = levelData.entities[i];
			if (!e.name.empty())
				entityNameMap.emplace(e.name, i);
		}

		for (const auto& e : levelData.entities)
		{
			if (e.archetype != "#BPM_CHANGE")
				continue;
			Tempo tempo;
			if (fromBpmChangeEntity(e, tempo))
				score.tempoChanges.push_back(tempo);
		}
		if (score.tempoChanges.empty())
			score.tempoChanges.push_back(Tempo(0, 120.f));

		std::unordered_map<RefType, size_t> groupNameMap;
		for (const auto& e : levelData.entities)
		{
			if (e.archetype != "#TIMESCALE_GROUP")
				continue;
			size_t layerIdx = score.layers.size();
			Layer& layer = score.layers.emplace_back();
			layer.name = layerIdx == 0 ? "default" : ("#" + std::to_string(layerIdx));
			layer.hidden = false;
			if (!e.name.empty())
				groupNameMap.emplace(e.name, layerIdx);

			RefType next;
			e.tryGetDataValue("first", next);
			while (!next.empty())
			{
				auto it = entityNameMap.find(next);
				if (it == entityNameMap.end())
					break;
				const auto& hsEnt = levelData.entities[it->second];
				HiSpeedChange hs;
				hs.ID = getNextHiSpeedID();
				float beat;
				if (hsEnt.tryGetDataValue("#BEAT", beat))
					hs.tick = beatsToTicks(beat);
				hsEnt.tryGetDataValue("#TIMESCALE", hs.speed);
				hsEnt.tryGetDataValue("#TIMESCALE_SKIP", hs.skips);
				int ease = 0;
				hsEnt.tryGetDataValue("#TIMESCALE_EASE", ease);
				hs.ease = static_cast<HiSpeedEaseType>(ease);
				int hideNotes = 0;
				hsEnt.tryGetDataValue("hideNotes", hideNotes);
				hs.hideNotes = hideNotes != 0;
				hs.layer = static_cast<int>(layerIdx);
				score.hiSpeedChanges[hs.ID] = hs;

				next.clear();
				hsEnt.tryGetDataValue("next", next);
			}
		}
		if (score.layers.empty())
			score.layers.push_back(Layer{ "default" });

		for (const auto& e : levelData.entities)
		{
			if (e.archetype == "Skill")
			{
				SkillTrigger skill;
				skill.ID = getNextSkillID();
				float beat;
				if (e.tryGetDataValue("#BEAT", beat))
					skill.tick = beatsToTicks(beat);
				score.skills[skill.ID] = skill;
			}
			else if (e.archetype == "FeverChance")
			{
				float beat;
				if (e.tryGetDataValue("#BEAT", beat))
					score.fever.startTick = beatsToTicks(beat);
			}
			else if (e.archetype == "FeverStart")
			{
				float beat;
				if (e.tryGetDataValue("#BEAT", beat))
					score.fever.endTick = beatsToTicks(beat);
			}
		}

		std::unordered_map<RefType, size_t> notePrevMap;
		std::vector<size_t> noteEntities;
		for (size_t i = 0; i < levelData.entities.size(); ++i)
		{
			const auto& e = levelData.entities[i];
			if (!IO::endsWith(e.archetype, "Note") ||
			    e.archetype.find("Transient") != std::string::npos)
				continue;
			noteEntities.push_back(i);
			RefType next;
			if (e.tryGetDataValue("next", next))
				notePrevMap.emplace(next, i);
		}

		id_t nextNoteID = 0;
		id_t nextHoldID = 0;

		const auto hasParent = [&](const LevelDataEntity& e)
		{ return e.name.size() && notePrevMap.find(e.name) != notePrevMap.end(); };

		for (size_t entIdx : noteEntities)
		{
			const auto& e = levelData.entities[entIdx];
			if (e.dataExists("next") || hasParent(e))
				continue;
			Note note;
			if (!fromTapNoteEntity(e, note, groupNameMap))
				continue;
			note.ID = nextNoteID++;
			score.notes.emplace(note.ID, note);
		}

		for (size_t entIdx : noteEntities)
		{
			const auto& startEnt = levelData.entities[entIdx];
			if (!startEnt.dataExists("next") || hasParent(startEnt))
				continue;

			HoldNote hold;
			std::vector<Note> holdNotes;
			size_t curIdx = entIdx;
			bool valid = true;

			while (true)
			{
				const auto& cur = levelData.entities[curIdx];
				Note n;
				if (!fromHoldMidEntity(cur, n, groupNameMap))
				{
					valid = false;
					break;
				}
				holdNotes.push_back(n);

				RefType next;
				if (!cur.tryGetDataValue("next", next))
					break;
				auto it = entityNameMap.find(next);
				if (it == entityNameMap.end())
				{
					valid = false;
					break;
				}
				curIdx = it->second;
			}

			if (!valid || holdNotes.size() < 2)
				continue;

			for (auto& hn : holdNotes)
			{
				hn.ID = nextNoteID++;
				score.notes.emplace(hn.ID, hn);
			}

			hold.start.ID = holdNotes.front().ID;
			hold.start.type = HoldStepType::Normal;
			hold.start.ease = holdNotes.front().flick != FlickType::None ? EaseType::Linear : EaseType::Linear;
			hold.end = holdNotes.back().ID;
			hold.startType = HoldNoteType::Normal;
			hold.endType = HoldNoteType::Normal;
			hold.fadeType = FadeType::Out;

			for (size_t i = 1; i + 1 < holdNotes.size(); ++i)
			{
				HoldStep step;
				step.ID = holdNotes[i].ID;
				step.type = HoldStepType::Normal;
				step.ease = EaseType::Linear;
				int ease;
				const auto& midEnt = levelData.entities[noteEntities[i]];
				if (midEnt.tryGetDataValue("connectorEase", ease))
					step.ease = fromEaseNumeric(ease);
				hold.steps.push_back(step);
			}

			hold.dummy = false;
			id_t hid = nextHoldID++;
			score.holdNotes.emplace(hid, hold);
		}

		return score;
	}

	bool PySekaiEngine::canSerialize(const Score&) { return true; }

	LevelDataEntity PySekaiEngine::toGroupEntity(const Layer& layer)
	{
		return { "#TIMESCALE_GROUP", { { "editorName", layer.name } } };
	}

	LevelDataEntity PySekaiEngine::toTimeScaleEntity(const HiSpeedChange& hispeed,
	                                                  const RefType& groupName)
	{
		return { "#TIMESCALE_CHANGE",
			     { { "#BEAT", ticksToBeats(hispeed.tick) },
			       { "#TIMESCALE", static_cast<RealType>(hispeed.speed) },
			       { "#TIMESCALE_SKIP", static_cast<RealType>(hispeed.skips) },
			       { "#TIMESCALE_EASE", static_cast<IntegerType>(hispeed.ease) },
			       { "#TIMESCALE_GROUP", groupName },
			       { "hideNotes", static_cast<IntegerType>(hispeed.hideNotes ? 1 : 0) } } };
	}

	Sonolus::LevelDataEntity PySekaiEngine::toSkillEntity(const SkillTrigger& skill)
	{
		return { "Skill", { { "#BEAT", ticksToBeats(skill.tick) } } };
	}

	std::pair<Sonolus::LevelDataEntity, Sonolus::LevelDataEntity>
	PySekaiEngine::toFeverEntities(const Fever& fever)
	{
		return { { "FeverChance", { { "#BEAT", ticksToBeats(fever.startTick) } } },
			     { "FeverStart", { { "#BEAT", ticksToBeats(fever.endTick) } } } };
	}

	LevelDataEntity PySekaiEngine::toNoteEntity(const Note& note, const std::string& archetype,
	                                             const RefType& groupName, const HoldNote* hold,
	                                             const HoldStep* holdStep)
	{
		return { archetype,
			     { { "#TIMESCALE_GROUP", groupName },
			       { "#BEAT", ticksToBeats(note.tick) },
			       { "lane", toSonolusLane(note.lane, note.width) },
			       { "size", widthToSize(note.width) },
			       { "direction", toDirectionNumeric(note.flick) },
			       { "connectorEase", holdStep ? toEaseNumeric(holdStep->ease) : 1 },
			       { "isSeparator", static_cast<IntegerType>(1) } } };
	}

	std::string PySekaiEngine::getSingleNoteArchetype(const Note& note)
	{
		std::string arch;
		switch (note.getType())
		{
		case NoteType::Tap:
			arch = note.critical ? "CriticalNormalTapNote" : "NormalTapNote";
			if (note.isFlick())
				arch = (note.critical ? "CriticalNormalFlickNote" : "NormalFlickNote");
			break;
		case NoteType::Damage:
			arch = "DamageNote";
			break;
		default:
			arch = "AnchorNote";
			break;
		}
		return arch;
	}

	std::string PySekaiEngine::getHoldNoteArchetype(const Note& note, bool isHead, bool isTail)
	{
		if (note.isFlick() && isTail)
			return note.critical ? "CriticalNormalFlickNote" : "NormalFlickNote";
		if (isHead)
			return note.critical ? "CriticalNormalHeadTapNote" : "NormalHeadTapNote";
		if (isTail)
			return note.critical ? "CriticalNormalTailReleaseNote" : "NormalTailReleaseNote";
		return note.critical ? "CriticalNormalTickNote" : "NormalTickNote";
	}

	void PySekaiEngine::insertTransientTickNote(size_t, size_t, bool,
	                                             std::vector<Sonolus::LevelDataEntity>&) {}

	void PySekaiEngine::estimateAttachEntity(Sonolus::LevelDataEntity&,
	                                          const Sonolus::LevelDataEntity&,
	                                          const Sonolus::LevelDataEntity&) {}

	int PySekaiEngine::toDirectionNumeric(FlickType flick)
	{
		switch (flick)
		{
		case FlickType::Left:   return 1;
		case FlickType::Right:  return 2;
		case FlickType::Down:   return 3;
		case FlickType::DownLeft:  return 4;
		case FlickType::DownRight: return 5;
		default:                return 0;
		}
	}

	int PySekaiEngine::toEaseNumeric(EaseType ease)
	{
		switch (ease)
		{
		case EaseType::Linear:   return 1;
		case EaseType::EaseIn:   return 2;
		case EaseType::EaseOut:  return 3;
		case EaseType::EaseInOut: return 4;
		case EaseType::EaseOutIn: return 5;
		default:                  return 1;
		}
	}

	int PySekaiEngine::toSegmentNumeric(const HoldStep* holdStep)
	{
		(void)holdStep;
		return 1;
	}

	bool PySekaiEngine::fromGroupEntity(const Sonolus::LevelDataEntity& groupEntity, Layer& layer)
	{
		groupEntity.tryGetDataValue("editorName", layer.name);
		return true;
	}

	bool PySekaiEngine::fromTimeScaleEntity(const Sonolus::LevelDataEntity& timescaleEntity,
	                                         const Sonolus::LevelDataEntity& groupEntity,
	                                         HiSpeedChange& hispeed,
	                                         std::unordered_map<RefType, size_t>& groupNameMap)
	{
		float beat;
		if (!timescaleEntity.tryGetDataValue("#BEAT", beat))
			return false;
		hispeed.tick = beatsToTicks(beat);
		timescaleEntity.tryGetDataValue("#TIMESCALE", hispeed.speed);
		timescaleEntity.tryGetDataValue("#TIMESCALE_SKIP", hispeed.skips);
		int ease = 0;
		timescaleEntity.tryGetDataValue("#TIMESCALE_EASE", ease);
		hispeed.ease = static_cast<HiSpeedEaseType>(ease);
		int hideNotes = 0;
		timescaleEntity.tryGetDataValue("hideNotes", hideNotes);
		hispeed.hideNotes = hideNotes != 0;
		(void)groupEntity;
		(void)groupNameMap;
		return true;
	}

	bool PySekaiEngine::fromSkillEntity(const Sonolus::LevelDataEntity& skillEntity,
	                                     SkillTrigger& skill)
	{
		float beat;
		if (!skillEntity.tryGetDataValue("#BEAT", beat))
			return false;
		skill.tick = beatsToTicks(beat);
		return true;
	}

	bool PySekaiEngine::fromFeverEntity(const Sonolus::LevelDataEntity& feverEntity, Fever& fever)
	{
		float beat;
		if (!feverEntity.tryGetDataValue("#BEAT", beat))
			return false;
		if (feverEntity.archetype == "FeverChance")
			fever.startTick = beatsToTicks(beat);
		else
			fever.endTick = beatsToTicks(beat);
		return true;
	}

	int PySekaiEngine::fromNoteEntity(const Sonolus::LevelDataEntity& noteEntity, Note& note,
	                                   const std::unordered_map<RefType, size_t>& groupNameMap)
	{
		float beat, lane, size;
		if (!noteEntity.tryGetDataValue("#BEAT", beat) ||
		    !noteEntity.tryGetDataValue("lane", lane) ||
		    !noteEntity.tryGetDataValue("size", size))
			return 0;
		note.tick = beatsToTicks(beat);
		note.width = sizeToWidth(size);
		note.lane = toNativeLane(lane, size);
		note.layer = 0;
		std::string group;
		if (noteEntity.tryGetDataValue("#TIMESCALE_GROUP", group))
		{
			auto it = groupNameMap.find(group);
			if (it != groupNameMap.end())
				note.layer = static_cast<int>(it->second);
		}
		return 1;
	}

	int PySekaiEngine::fromTapNoteEntity(const Sonolus::LevelDataEntity& tapNoteEntity, Note& note,
	                                      const std::unordered_map<RefType, size_t>& groupNameMap)
	{
		int status = fromNoteEntity(tapNoteEntity, note, groupNameMap);
		if (!status)
			return 0;
		const std::string& arch = tapNoteEntity.archetype;
		if (arch.find("Critical") != std::string::npos)
			note.critical = true;
		if (arch.find("Flick") != std::string::npos)
		{
			int dir = 0;
			tapNoteEntity.tryGetDataValue("direction", dir);
			note.flick = fromDirectionNumeric(dir);
		}
		if (arch.find("Damage") != std::string::npos)
		{
			note = Note(NoteType::Damage, note.tick, note.lane, note.width);
		}
		else if (arch.find("Tick") != std::string::npos)
		{
			note = Note(NoteType::HoldMid, note.tick, note.lane, note.width);
			note.critical = arch.find("Critical") != std::string::npos;
		}
		else
		{
			note = Note(NoteType::Tap, note.tick, note.lane, note.width);
			note.critical = arch.find("Critical") != std::string::npos;
			if (arch.find("Flick") != std::string::npos)
			{
				int dir = 0;
				tapNoteEntity.tryGetDataValue("direction", dir);
				note.flick = fromDirectionNumeric(dir);
			}
		}
		return status;
	}

	int PySekaiEngine::fromHoldMidEntity(const Sonolus::LevelDataEntity& noteEntity, Note& note,
	                                      const std::unordered_map<RefType, size_t>& groupNameMap)
	{
		return fromTapNoteEntity(noteEntity, note, groupNameMap);
	}

	bool PySekaiEngine::isValidNoteState(const Note& note)
	{
		return note.tick >= 0 && note.width >= 0;
	}

	bool PySekaiEngine::isValidHoldNotes(const std::vector<Note>& holdNotes, const HoldNote&)
	{
		return holdNotes.size() >= 2;
	}

	FlickType PySekaiEngine::fromDirectionNumeric(int direction)
	{
		switch (direction)
		{
		case 0: return FlickType::Default;
		case 1: return FlickType::Left;
		case 2: return FlickType::Right;
		case 3: return FlickType::Down;
		case 4: return FlickType::DownLeft;
		case 5: return FlickType::DownRight;
		default: return FlickType::None;
		}
	}

	EaseType PySekaiEngine::fromEaseNumeric(int ease)
	{
		switch (ease)
		{
		case 1: return EaseType::Linear;
		case 2: return EaseType::EaseIn;
		case 3: return EaseType::EaseOut;
		case 4: return EaseType::EaseInOut;
		case 5: return EaseType::EaseOutIn;
		default: return EaseType::Linear;
		}
	}

	bool PySekaiEngine::fromSegmentNumeric(int, HoldStep&) { return true; }
}
