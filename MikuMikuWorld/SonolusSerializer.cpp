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

	static bool tryGetGuideColorFromSegmentKind(const Sonolus::LevelDataEntity& e,
	                                            GuideColor& outColor)
	{
		int kind = 0;
		if (!e.tryGetDataValue("segmentKind", kind))
			return false;
		if (kind < 101 || kind > 108)
			return false;
		outColor = static_cast<GuideColor>(kind - 101);
		return true;
	}

	static void applyHiddenOrGuideInfo(HoldNote& hold, const std::vector<size_t>& holdEntIndices,
	                                   const std::vector<Sonolus::LevelDataEntity>& entities)
	{
		GuideColor guideColor;
		bool anyGuide = false;
		for (size_t idx : holdEntIndices)
		{
			if (tryGetGuideColorFromSegmentKind(entities[idx], guideColor))
			{
				anyGuide = true;
				break;
			}
		}
		if (anyGuide)
		{
			hold.startType = HoldNoteType::Guide;
			hold.endType = HoldNoteType::Guide;
			hold.guideColor = guideColor;
			return;
		}

		if (entities[holdEntIndices.front()].archetype.find("Anchor") != std::string::npos)
			hold.startType = HoldNoteType::Hidden;
		if (entities[holdEntIndices.back()].archetype.find("Anchor") != std::string::npos)
			hold.endType = HoldNoteType::Hidden;
		for (size_t i = 1; i + 1 < holdEntIndices.size(); ++i)
		{
			if (entities[holdEntIndices[i]].archetype.find("Anchor") != std::string::npos)
				hold.steps[i - 1].type = HoldStepType::Hidden;
		}
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
			     { { "#BEAT", ticksToBeats(tempo.tick) },
			       { "#BPM", static_cast<RealType>(tempo.bpm) } } };
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

		size_t initIdx = levelData.entities.size();
		levelData.entities.emplace_back(toInitializationEntity());
		levelData.entities[initIdx].data["initialLife"] =
		    static_cast<IntegerType>(score.metadata.baseLifePoint);

		std::unordered_map<id_t, size_t> stageEntityIndex;
		for (const auto& [id, stage] : score.stages)
		{
			size_t idx = levelData.entities.size();
			levelData.entities.emplace_back(toStageEntity(stage));
			levelData.entities[idx].name = IO::formatString("stage%d", (int)idx);
			stageEntityIndex[id] = idx;
		}

		{
			std::vector<const CameraChangeEvent*> cameraList;
			cameraList.reserve(score.cameraChanges.size());
			for (const auto& [_, c] : score.cameraChanges)
				cameraList.push_back(&c);
			std::sort(cameraList.begin(), cameraList.end(),
			          [](const CameraChangeEvent* a, const CameraChangeEvent* b)
			          { return a->tick < b->tick; });

			size_t prevIdx = initIdx;
			for (const CameraChangeEvent* camera : cameraList)
			{
				size_t newIdx = levelData.entities.size();
				levelData.entities.emplace_back(toCameraChangeEntity(*camera));
				std::string newName = IO::formatString("camera%d", (int)newIdx);
				levelData.entities[newIdx].name = newName;

				if (prevIdx == initIdx)
					levelData.entities[prevIdx].data["firstCamera"] = newName;
				else
					levelData.entities[prevIdx].data["next"] = newName;

				prevIdx = newIdx;
			}
		}

		for (const auto& [stageID, stageIdx] : stageEntityIndex)
		{
			std::vector<const StageMaskChangeEvent*> maskList;
			for (const auto& [_, mask] : score.stageMaskChanges)
				if (mask.stageID == stageID)
					maskList.push_back(&mask);
			std::sort(maskList.begin(), maskList.end(),
			          [](const StageMaskChangeEvent* a, const StageMaskChangeEvent* b)
			          { return a->tick < b->tick; });

			std::string stageName = levelData.entities[stageIdx].name;
			size_t prevIdx = stageIdx;
			for (const StageMaskChangeEvent* mask : maskList)
			{
				size_t newIdx = levelData.entities.size();
				levelData.entities.emplace_back(toStageMaskChangeEntity(*mask));
				levelData.entities[newIdx].data["stage"] = stageName;
				std::string newName = IO::formatString("mask%d", (int)newIdx);
				levelData.entities[newIdx].name = newName;

				if (prevIdx == stageIdx)
					levelData.entities[prevIdx].data["firstMaskChange"] = newName;
				else
					levelData.entities[prevIdx].data["next"] = newName;

				prevIdx = newIdx;
			}
		}

		for (const auto& [stageID, stageIdx] : stageEntityIndex)
		{
			std::vector<const StagePivotChangeEvent*> pivotList;
			for (const auto& [_, pivot] : score.stagePivotChanges)
				if (pivot.stageID == stageID)
					pivotList.push_back(&pivot);
			std::sort(pivotList.begin(), pivotList.end(),
			          [](const StagePivotChangeEvent* a, const StagePivotChangeEvent* b)
			          { return a->tick < b->tick; });

			std::string stageName = levelData.entities[stageIdx].name;
			size_t prevIdx = stageIdx;
			for (const StagePivotChangeEvent* pivot : pivotList)
			{
				size_t newIdx = levelData.entities.size();
				levelData.entities.emplace_back(toStagePivotChangeEntity(*pivot));
				levelData.entities[newIdx].data["stage"] = stageName;
				std::string newName = IO::formatString("pivot%d", (int)newIdx);
				levelData.entities[newIdx].name = newName;

				if (prevIdx == stageIdx)
					levelData.entities[prevIdx].data["firstPivotChange"] = newName;
				else
					levelData.entities[prevIdx].data["next"] = newName;

				prevIdx = newIdx;
			}
		}

		for (const auto& [stageID, stageIdx] : stageEntityIndex)
		{
			std::vector<const StageStyleChangeEvent*> styleList;
			for (const auto& [_, style] : score.stageStyleChanges)
				if (style.stageID == stageID)
					styleList.push_back(&style);
			std::sort(styleList.begin(), styleList.end(),
			          [](const StageStyleChangeEvent* a, const StageStyleChangeEvent* b)
			          { return a->tick < b->tick; });

			std::string stageName = levelData.entities[stageIdx].name;
			size_t prevIdx = stageIdx;
			for (const StageStyleChangeEvent* style : styleList)
			{
				size_t newIdx = levelData.entities.size();
				levelData.entities.emplace_back(toStageStyleChangeEntity(*style));
				levelData.entities[newIdx].data["stage"] = stageName;
				std::string newName = IO::formatString("style%d", (int)newIdx);
				levelData.entities[newIdx].name = newName;

				if (prevIdx == stageIdx)
					levelData.entities[prevIdx].data["firstStyleChange"] = newName;
				else
					levelData.entities[prevIdx].data["next"] = newName;

				prevIdx = newIdx;
			}
		}

		levelData.entities.emplace_back(
		    "#BPM_CHANGE",
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
			size_t groupIdx =
			    layerGroupIndex.count(layer) ? layerGroupIndex[layer] : layerGroupIndex[0];
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
			size_t groupIdx =
			    layerGroupIndex.count(layer) ? layerGroupIndex[layer] : layerGroupIndex[0];
			std::string groupName;
			if (!levelData.entities[groupIdx].name.empty())
				groupName = levelData.entities[groupIdx].name;
			else
			{
				levelData.entities[groupIdx].name = IO::formatString("g%d", layer);
				groupName = levelData.entities[groupIdx].name;
			}
			size_t entIdx = levelData.entities.size();
			RefType noteStageRef;
			auto noteStageIt = stageEntityIndex.find(note.stageID);
			if (noteStageIt != stageEntityIndex.end())
				noteStageRef = levelData.entities[noteStageIt->second].name;
			float notePivotLane = getActiveStagePivotLane(score, note.stageID, note.tick);
			levelData.entities.emplace_back(toNoteEntity(note, getSingleNoteArchetype(note),
			                                             groupName, noteStageRef, notePivotLane));
			simBuilder.emplace(note.tick, entIdx);
		}

		for (const auto& [holdID, hold] : score.holdNotes)
		{
			const Note& startNote = score.notes.at(hold.start.ID);
			int layer = startNote.layer;
			size_t groupIdx =
			    layerGroupIndex.count(layer) ? layerGroupIndex[layer] : layerGroupIndex[0];
			std::string groupName;
			if (!levelData.entities[groupIdx].name.empty())
				groupName = levelData.entities[groupIdx].name;
			else
			{
				levelData.entities[groupIdx].name = IO::formatString("g%d", layer);
				groupName = levelData.entities[groupIdx].name;
			}

			size_t startIdx = levelData.entities.size();
			RefType startStageRef;
			auto startStageIt = stageEntityIndex.find(startNote.stageID);
			if (startStageIt != stageEntityIndex.end())
				startStageRef = levelData.entities[startStageIt->second].name;
			float startPivotLane =
			    getActiveStagePivotLane(score, startNote.stageID, startNote.tick);
			bool startIsAnchor = hold.isGuide() || hold.startType != HoldNoteType::Normal;
			levelData.entities.emplace_back(
			    toNoteEntity(startNote, getHoldNoteArchetype(startNote, true, false, startIsAnchor),
			                 groupName, startStageRef, startPivotLane, &hold, &hold.start));
			std::string startName = IO::formatString("n%d", (int)startIdx);
			levelData.entities[startIdx].name = startName;
			simBuilder.emplace(startNote.tick, startIdx);

			size_t prevIdx = startIdx;
			for (int i = 0; i < (int)hold.steps.size(); ++i)
			{
				const HoldStep& step = hold.steps[i];
				const Note& midNote = score.notes.at(step.ID);
				int midLayer = midNote.layer;
				size_t midGroupIdx = layerGroupIndex.count(midLayer) ? layerGroupIndex[midLayer]
				                                                     : layerGroupIndex[0];
				std::string midGroupName;
				if (!levelData.entities[midGroupIdx].name.empty())
					midGroupName = levelData.entities[midGroupIdx].name;
				else
				{
					levelData.entities[midGroupIdx].name = IO::formatString("g%d", midLayer);
					midGroupName = levelData.entities[midGroupIdx].name;
				}

				size_t midIdx = levelData.entities.size();
				RefType midStageRef;
				auto midStageIt = stageEntityIndex.find(midNote.stageID);
				if (midStageIt != stageEntityIndex.end())
					midStageRef = levelData.entities[midStageIt->second].name;
				float midPivotLane = getActiveStagePivotLane(score, midNote.stageID, midNote.tick);
				bool midIsAnchor = hold.isGuide() || step.type == HoldStepType::Hidden;
				levelData.entities.emplace_back(
				    toNoteEntity(midNote, getHoldNoteArchetype(midNote, false, false, midIsAnchor),
				                 midGroupName, midStageRef, midPivotLane, &hold, &step));
				std::string midName = IO::formatString("n%d", (int)midIdx);
				levelData.entities[midIdx].name = midName;
				levelData.entities[prevIdx].data["next"] = midName;
				simBuilder.emplace(midNote.tick, midIdx);
				prevIdx = midIdx;
			}

			const Note& endNote = score.notes.at(hold.end);
			int endLayer = endNote.layer;
			size_t endGroupIdx =
			    layerGroupIndex.count(endLayer) ? layerGroupIndex[endLayer] : layerGroupIndex[0];
			std::string endGroupName;
			if (!levelData.entities[endGroupIdx].name.empty())
				endGroupName = levelData.entities[endGroupIdx].name;
			else
			{
				levelData.entities[endGroupIdx].name = IO::formatString("g%d", endLayer);
				endGroupName = levelData.entities[endGroupIdx].name;
			}

			size_t endIdx = levelData.entities.size();
			RefType endStageRef;
			auto endStageIt = stageEntityIndex.find(endNote.stageID);
			if (endStageIt != stageEntityIndex.end())
				endStageRef = levelData.entities[endStageIt->second].name;
			float endPivotLane = getActiveStagePivotLane(score, endNote.stageID, endNote.tick);
			bool endIsAnchor = hold.isGuide() || hold.endType != HoldNoteType::Normal;
			levelData.entities.emplace_back(
			    toNoteEntity(endNote, getHoldNoteArchetype(endNote, false, true, endIsAnchor),
			                 endGroupName, endStageRef, endPivotLane, &hold, nullptr));
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
				levelData.entities.push_back(
				    { "SimLine",
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

		std::unordered_map<RefType, id_t> stageNameToID;
		for (const auto& e : levelData.entities)
		{
			if (e.archetype != "Stage")
				continue;
			Stage stage;
			fromStageEntity(e, stage);
			stage.ID = getNextStageID();
			score.stages[stage.ID] = stage;
			score.stageOrder.push_back(stage.ID);
			if (!e.name.empty())
				stageNameToID[e.name] = stage.ID;
		}

		for (const auto& e : levelData.entities)
		{
			if (e.archetype != "Stage" || e.name.empty())
				continue;
			auto stageIt = stageNameToID.find(e.name);
			if (stageIt == stageNameToID.end())
				continue;
			id_t stageID = stageIt->second;

			RefType nextMask;
			if (e.tryGetDataValue("firstMaskChange", nextMask))
			{
				while (!nextMask.empty())
				{
					auto it = entityNameMap.find(nextMask);
					if (it == entityNameMap.end())
						break;
					const auto& maskEnt = levelData.entities[it->second];
					StageMaskChangeEvent mask;
					if (fromStageMaskChangeEntity(maskEnt, mask))
					{
						mask.ID = getNextStageMaskChangeID();
						mask.stageID = stageID;
						score.stageMaskChanges[mask.ID] = mask;
					}
					nextMask.clear();
					maskEnt.tryGetDataValue("next", nextMask);
				}
			}

			RefType nextPivot;
			if (e.tryGetDataValue("firstPivotChange", nextPivot))
			{
				while (!nextPivot.empty())
				{
					auto it = entityNameMap.find(nextPivot);
					if (it == entityNameMap.end())
						break;
					const auto& pivotEnt = levelData.entities[it->second];
					StagePivotChangeEvent pivot;
					if (fromStagePivotChangeEntity(pivotEnt, pivot))
					{
						pivot.ID = getNextStagePivotChangeID();
						pivot.stageID = stageID;
						score.stagePivotChanges[pivot.ID] = pivot;
					}
					nextPivot.clear();
					pivotEnt.tryGetDataValue("next", nextPivot);
				}
			}

			RefType nextStyle;
			if (e.tryGetDataValue("firstStyleChange", nextStyle))
			{
				while (!nextStyle.empty())
				{
					auto it = entityNameMap.find(nextStyle);
					if (it == entityNameMap.end())
						break;
					const auto& styleEnt = levelData.entities[it->second];
					StageStyleChangeEvent style;
					if (fromStageStyleChangeEntity(styleEnt, style))
					{
						style.ID = getNextStageStyleChangeID();
						style.stageID = stageID;
						score.stageStyleChanges[style.ID] = style;
					}
					nextStyle.clear();
					styleEnt.tryGetDataValue("next", nextStyle);
				}
			}
		}

		for (const auto& e : levelData.entities)
		{
			if (e.archetype != "Initialization")
				continue;
			RefType nextCamera;
			if (!e.tryGetDataValue("firstCamera", nextCamera))
				break;
			while (!nextCamera.empty())
			{
				auto it = entityNameMap.find(nextCamera);
				if (it == entityNameMap.end())
					break;
				const auto& cameraEnt = levelData.entities[it->second];
				CameraChangeEvent camera;
				if (fromCameraChangeEntity(cameraEnt, camera))
				{
					camera.ID = getNextCameraChangeID();
					score.cameraChanges[camera.ID] = camera;
				}
				nextCamera.clear();
				cameraEnt.tryGetDataValue("next", nextCamera);
			}
			break;
		}

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
		std::unordered_map<RefType, size_t> activeHeadMap;
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
			RefType activeHead;
			if (e.tryGetDataValue("activeHead", activeHead))
				activeHeadMap.emplace(activeHead, i);
		}

		const bool usesActiveHead = !activeHeadMap.empty() && notePrevMap.empty();

		id_t nextNoteID = 0;
		id_t nextHoldID = 0;

		const auto hasParent = [&](const LevelDataEntity& e)
		{ return e.name.size() && notePrevMap.find(e.name) != notePrevMap.end(); };

		const auto hasTail = [&](const LevelDataEntity& e)
		{ return e.name.size() && activeHeadMap.find(e.name) != activeHeadMap.end(); };

		if (!usesActiveHead)
		{
			for (size_t entIdx : noteEntities)
			{
				const auto& e = levelData.entities[entIdx];
				if (e.dataExists("next") || hasParent(e))
					continue;
				Note note;
				if (!fromTapNoteEntity(e, note, groupNameMap, stageNameToID))
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
				std::vector<size_t> holdEntIndices;
				size_t curIdx = entIdx;
				bool valid = true;

				while (true)
				{
					const auto& cur = levelData.entities[curIdx];
					bool isFirst = holdNotes.empty();
					RefType peekNext;
					bool hasNext = cur.tryGetDataValue("next", peekNext);
					NoteType posType = isFirst ? NoteType::Hold
					                           : (hasNext ? NoteType::HoldMid : NoteType::HoldEnd);
					Note n;
					if (!fromHoldMidEntity(cur, n, groupNameMap, stageNameToID, posType))
					{
						valid = false;
						break;
					}
					holdNotes.push_back(n);
					holdEntIndices.push_back(curIdx);

					if (!hasNext)
						break;
					auto it = entityNameMap.find(peekNext);
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
				hold.start.ease = EaseType::Linear;
				{
					int startEase;
					if (levelData.entities[holdEntIndices[0]].tryGetDataValue("connectorEase",
					                                                          startEase))
						hold.start.ease = fromEaseNumeric(startEase);
				}
				hold.end = holdNotes.back().ID;
				hold.startType = HoldNoteType::Normal;
				hold.endType = HoldNoteType::Normal;
				hold.fadeType = FadeType::Out;
				{
					int fadeVal;
					if (levelData.entities[holdEntIndices[0]].tryGetDataValue("fadeType", fadeVal))
						hold.fadeType = fromFadeNumeric(fadeVal);
				}

				for (size_t i = 1; i + 1 < holdNotes.size(); ++i)
				{
					HoldStep step;
					step.ID = holdNotes[i].ID;
					step.type = HoldStepType::Normal;
					step.ease = EaseType::Linear;
					int ease;
					const auto& midEnt = levelData.entities[holdEntIndices[i]];
					if (midEnt.tryGetDataValue("connectorEase", ease))
						step.ease = fromEaseNumeric(ease);
					hold.steps.push_back(step);
				}

				hold.dummy = false;
				applyHiddenOrGuideInfo(hold, holdEntIndices, levelData.entities);
				score.holdNotes.emplace(hold.start.ID, hold);
				// Rebuild parentID
				score.notes[hold.start.ID].parentID = hold.start.ID;
				for (const auto& step : hold.steps)
					score.notes[step.ID].parentID = hold.start.ID;
				score.notes[hold.end].parentID = hold.start.ID;
			}
		}
		else
		{
			for (size_t entIdx : noteEntities)
			{
				const auto& e = levelData.entities[entIdx];
				if (e.dataExists("activeHead") || hasTail(e))
					continue;
				Note note;
				if (!fromTapNoteEntity(e, note, groupNameMap, stageNameToID))
					continue;
				note.ID = nextNoteID++;
				score.notes.emplace(note.ID, note);
			}

			for (size_t entIdx : noteEntities)
			{
				const auto& tailEnt = levelData.entities[entIdx];
				RefType activeHead;
				if (!tailEnt.tryGetDataValue("activeHead", activeHead))
					continue;

				auto headIt = entityNameMap.find(activeHead);
				if (headIt == entityNameMap.end())
					continue;

				size_t headIdx = headIt->second;
				const auto& headEnt = levelData.entities[headIdx];

				std::vector<Note> holdNotes;
				std::vector<size_t> holdEntIndices;

				Note headNote;
				if (!fromTapNoteEntity(headEnt, headNote, groupNameMap, stageNameToID,
				                       NoteType::Hold))
					continue;
				holdNotes.push_back(headNote);
				holdEntIndices.push_back(headIdx);

				for (size_t midIdx : noteEntities)
				{
					const auto& midEnt = levelData.entities[midIdx];
					if (midIdx == headIdx || midIdx == entIdx)
						continue;
					RefType midHead;
					if (!midEnt.tryGetDataValue("activeHead", midHead))
						continue;
					if (midHead == activeHead)
					{
						Note midNote;
						if (fromHoldMidEntity(midEnt, midNote, groupNameMap, stageNameToID))
						{
							holdNotes.push_back(midNote);
							holdEntIndices.push_back(midIdx);
						}
					}
				}

				Note tailNote;
				if (!fromTapNoteEntity(tailEnt, tailNote, groupNameMap, stageNameToID,
				                       NoteType::HoldEnd))
					continue;
				holdNotes.push_back(tailNote);
				holdEntIndices.push_back(entIdx);

				if (holdNotes.size() < 2)
					continue;

				std::sort(holdNotes.begin(), holdNotes.end(),
				          [](const Note& a, const Note& b) { return a.tick < b.tick; });

				for (auto& hn : holdNotes)
				{
					hn.ID = nextNoteID++;
					score.notes.emplace(hn.ID, hn);
				}

				HoldNote hold;
				hold.start.ID = holdNotes.front().ID;
				hold.start.type = HoldStepType::Normal;
				hold.start.ease = EaseType::Linear;
				{
					int startEase;
					if (levelData.entities[holdEntIndices[0]].tryGetDataValue("connectorEase",
					                                                          startEase))
						hold.start.ease = fromEaseNumeric(startEase);
				}
				hold.end = holdNotes.back().ID;
				hold.startType = HoldNoteType::Normal;
				hold.endType = HoldNoteType::Normal;
				hold.fadeType = FadeType::Out;
				{
					int fadeVal;
					if (levelData.entities[holdEntIndices[0]].tryGetDataValue("fadeType", fadeVal))
						hold.fadeType = fromFadeNumeric(fadeVal);
				}
				hold.dummy = false;

				for (size_t i = 1; i + 1 < holdNotes.size(); ++i)
				{
					HoldStep step;
					step.ID = holdNotes[i].ID;
					step.type = HoldStepType::Normal;
					step.ease = EaseType::Linear;
					int ease;
					const auto& midEnt = levelData.entities[holdEntIndices[i]];
					if (midEnt.tryGetDataValue("connectorEase", ease))
						step.ease = fromEaseNumeric(ease);
					hold.steps.push_back(step);
				}

				applyHiddenOrGuideInfo(hold, holdEntIndices, levelData.entities);
				score.holdNotes.emplace(hold.start.ID, hold);
				score.notes[hold.start.ID].parentID = hold.start.ID;
				for (const auto& step : hold.steps)
					score.notes[step.ID].parentID = hold.start.ID;
				score.notes[hold.end].parentID = hold.start.ID;
			}
		}

		for (auto& [_, note] : score.notes)
			note.lane += getActiveStagePivotLane(score, note.stageID, note.tick);

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

	LevelDataEntity PySekaiEngine::toInitializationEntity()
	{
		return { "Initialization", { { "initialLife", static_cast<IntegerType>(1000) } } };
	}

	Sonolus::LevelDataEntity PySekaiEngine::toSkillEntity(const SkillTrigger& skill)
	{
		return { "Skill", { { "#BEAT", ticksToBeats(skill.tick) } } };
	}

	LevelDataEntity PySekaiEngine::toStageEntity(const Stage& stage)
	{
		return { "Stage",
			     { { "editorName", RefType(stage.editorName) },
			       { "fromStart", static_cast<IntegerType>(stage.fromStart ? 1 : 0) },
			       { "untilEnd", static_cast<IntegerType>(stage.untilEnd ? 1 : 0) },
			       { "generateSimLines",
			         static_cast<IntegerType>(stage.generateSimLinesIsolated ? 1 : 0) } } };
	}

	LevelDataEntity PySekaiEngine::toCameraChangeEntity(const CameraChangeEvent& camera)
	{
		return { "CameraChange",
			     { { "#BEAT", ticksToBeats(camera.tick) },
			       { "lane", toSonolusLane(camera.left, camera.size) },
			       { "size", widthToSize(camera.size) },
			       { "zoom", static_cast<RealType>(camera.zoom) },
			       { "zoomTargetLane", static_cast<RealType>(camera.zoomTargetLane) },
			       { "zoomTargetY", static_cast<RealType>(camera.zoomTargetY) },
			       { "zoomVerticalAlign", static_cast<IntegerType>(camera.zoomVerticalAlign) },
			       { "rotate", static_cast<RealType>(camera.rotate) },
			       { "stageTilt", static_cast<RealType>(camera.stageTilt) },
			       { "ease", static_cast<IntegerType>(toEaseNumeric(camera.ease)) } } };
	}

	LevelDataEntity PySekaiEngine::toStageMaskChangeEntity(const StageMaskChangeEvent& mask)
	{
		return { "StageMaskChange",
			     { { "#BEAT", ticksToBeats(mask.tick) },
			       { "lane", toSonolusLane(mask.left, mask.size) },
			       { "size", widthToSize(mask.size) },
			       { "ease", static_cast<IntegerType>(toEaseNumeric(mask.ease)) } } };
	}

	LevelDataEntity PySekaiEngine::toStagePivotChangeEntity(const StagePivotChangeEvent& pivot)
	{
		return { "StagePivotChange",
			     { { "#BEAT", ticksToBeats(pivot.tick) },
			       { "lane", toSonolusLane(pivot.lane, 0) },
			       { "divisionSize", static_cast<IntegerType>(pivot.divisionSize) },
			       { "divisionParity", static_cast<IntegerType>(pivot.divisionParityOdd ? 1 : 0) },
			       { "yOffset", static_cast<RealType>(pivot.yOffset) },
			       { "yBeatOffset", static_cast<RealType>(pivot.yOffsetBeat) },
			       { "ease", static_cast<IntegerType>(toEaseNumeric(pivot.ease)) } } };
	}

	LevelDataEntity PySekaiEngine::toStageStyleChangeEntity(const StageStyleChangeEvent& style)
	{
		return { "StageStyleChange",
			     { { "#BEAT", ticksToBeats(style.tick) },
			       { "judgeLineColor", static_cast<IntegerType>(style.judgeLineColor) },
			       { "leftBorderStyle", static_cast<IntegerType>(style.leftBorderStyle) },
			       { "rightBorderStyle", static_cast<IntegerType>(style.rightBorderStyle) },
			       { "alpha", static_cast<RealType>(style.alpha) },
			       { "laneAlpha", static_cast<RealType>(style.laneAlpha) },
			       { "judgeLineAlpha", static_cast<RealType>(style.judgeLineAlpha) },
			       { "ease", static_cast<IntegerType>(toEaseNumeric(style.ease)) } } };
	}

	bool PySekaiEngine::fromStageEntity(const Sonolus::LevelDataEntity& e, Stage& stage)
	{
		e.tryGetDataValue("editorName", stage.editorName);
		int fromStart = 1, untilEnd = 1, simLines = 0;
		e.tryGetDataValue("fromStart", fromStart);
		e.tryGetDataValue("untilEnd", untilEnd);
		e.tryGetDataValue("generateSimLines", simLines);
		stage.fromStart = fromStart != 0;
		stage.untilEnd = untilEnd != 0;
		stage.generateSimLinesIsolated = simLines != 0;
		return true;
	}

	bool PySekaiEngine::fromCameraChangeEntity(const Sonolus::LevelDataEntity& e,
	                                           CameraChangeEvent& camera)
	{
		float beat, lane, size;
		if (!e.tryGetDataValue("#BEAT", beat) || !e.tryGetDataValue("lane", lane) ||
		    !e.tryGetDataValue("size", size))
			return false;
		camera.tick = beatsToTicks(beat);
		camera.size = sizeToWidth(size);
		camera.left = toNativeLane(lane, size);
		e.tryGetDataValue("zoom", camera.zoom);
		e.tryGetDataValue("zoomTargetLane", camera.zoomTargetLane);
		e.tryGetDataValue("zoomTargetY", camera.zoomTargetY);
		int align = 0, ease = 1;
		e.tryGetDataValue("zoomVerticalAlign", align);
		camera.zoomVerticalAlign = static_cast<StageZoomVerticalAlign>(align);
		e.tryGetDataValue("rotate", camera.rotate);
		e.tryGetDataValue("stageTilt", camera.stageTilt);
		e.tryGetDataValue("ease", ease);
		camera.ease = fromEaseNumeric(ease);
		return true;
	}

	bool PySekaiEngine::fromStageMaskChangeEntity(const Sonolus::LevelDataEntity& e,
	                                              StageMaskChangeEvent& mask)
	{
		float beat, lane, size;
		if (!e.tryGetDataValue("#BEAT", beat) || !e.tryGetDataValue("lane", lane) ||
		    !e.tryGetDataValue("size", size))
			return false;
		mask.tick = beatsToTicks(beat);
		mask.size = sizeToWidth(size);
		mask.left = toNativeLane(lane, size);
		int ease = 1;
		e.tryGetDataValue("ease", ease);
		mask.ease = fromEaseNumeric(ease);
		return true;
	}

	bool PySekaiEngine::fromStagePivotChangeEntity(const Sonolus::LevelDataEntity& e,
	                                               StagePivotChangeEvent& pivot)
	{
		float beat, lane;
		if (!e.tryGetDataValue("#BEAT", beat) || !e.tryGetDataValue("lane", lane))
			return false;
		pivot.tick = beatsToTicks(beat);
		pivot.lane = toNativeLane(lane, 0);
		int divisionSize = 2, divisionParity = 0, ease = 1;
		e.tryGetDataValue("divisionSize", divisionSize);
		e.tryGetDataValue("divisionParity", divisionParity);
		pivot.divisionSize = divisionSize;
		pivot.divisionParityOdd = divisionParity != 0;
		e.tryGetDataValue("yOffset", pivot.yOffset);
		e.tryGetDataValue("yBeatOffset", pivot.yOffsetBeat);
		e.tryGetDataValue("ease", ease);
		pivot.ease = fromEaseNumeric(ease);
		return true;
	}

	bool PySekaiEngine::fromStageStyleChangeEntity(const Sonolus::LevelDataEntity& e,
	                                               StageStyleChangeEvent& style)
	{
		float beat;
		if (!e.tryGetDataValue("#BEAT", beat))
			return false;
		style.tick = beatsToTicks(beat);
		int judge = static_cast<int>(GuideColor::Purple), left = 0, right = 0, ease = 1;
		e.tryGetDataValue("judgeLineColor", judge);
		e.tryGetDataValue("leftBorderStyle", left);
		e.tryGetDataValue("rightBorderStyle", right);
		e.tryGetDataValue("ease", ease);
		style.judgeLineColor = static_cast<GuideColor>(judge);
		style.leftBorderStyle = static_cast<StageBorderStyle>(left);
		style.rightBorderStyle = static_cast<StageBorderStyle>(right);
		style.alpha = style.laneAlpha = style.judgeLineAlpha = 1.f;
		e.tryGetDataValue("alpha", style.alpha);
		e.tryGetDataValue("laneAlpha", style.laneAlpha);
		e.tryGetDataValue("judgeLineAlpha", style.judgeLineAlpha);
		style.ease = fromEaseNumeric(ease);
		return true;
	}

	std::pair<Sonolus::LevelDataEntity, Sonolus::LevelDataEntity>
	PySekaiEngine::toFeverEntities(const Fever& fever)
	{
		return { { "FeverChance", { { "#BEAT", ticksToBeats(fever.startTick) } } },
			     { "FeverStart", { { "#BEAT", ticksToBeats(fever.endTick) } } } };
	}

	LevelDataEntity PySekaiEngine::toNoteEntity(const Note& note, const std::string& archetype,
	                                            const RefType& groupName, const RefType& stageRef,
	                                            float pivotLane, const HoldNote* hold,
	                                            const HoldStep* holdStep)
	{
		LevelDataEntity entity{ archetype,
			                    { { "#TIMESCALE_GROUP", groupName },
			                      { "#BEAT", ticksToBeats(note.tick) },
			                      { "lane", toSonolusLane(note.lane, note.width) - pivotLane },
			                      { "size", widthToSize(note.width) },
			                      { "direction", toDirectionNumeric(note.flick) },
			                      { "connectorEase", holdStep ? toEaseNumeric(holdStep->ease) : 1 },
			                      { "isSeparator", static_cast<IntegerType>(1) },
			                      { "isAttached", static_cast<IntegerType>(1) },
			                      { "segmentKind", toSegmentNumeric(note, hold) },
			                      { "segmentAlpha", 1.0 },
			                      { "segmentLayer", static_cast<IntegerType>(0) },
			                      { "effectKind", static_cast<IntegerType>(1) } } };
		if (hold)
			entity.data["fadeType"] = toFadeNumeric(hold->fadeType);
		if (!stageRef.empty())
			entity.data["stage"] = stageRef;
		return entity;
	}

	float PySekaiEngine::getActiveStagePivotLane(const Score& score, id_t stageID, int tick)
	{
		if (stageID == NO_ID)
			return 0;

		bool found = false;
		int bestTick = 0;
		float bestLane = 0;
		for (const auto& [_, pivot] : score.stagePivotChanges)
		{
			if (pivot.stageID != stageID || pivot.tick > tick)
				continue;
			if (!found || pivot.tick >= bestTick)
			{
				found = true;
				bestTick = pivot.tick;
				bestLane = pivot.lane;
			}
		}
		if (!found)
			return 0;
		return toSonolusLane(bestLane, 0);
	}

	std::string PySekaiEngine::getSingleNoteArchetype(const Note& note)
	{
		if (note.getType() == NoteType::Damage)
			return "DamageNote";

		std::string archetype = note.critical ? "Critical" : "Normal";
		if (note.friction)
			archetype += "Trace";
		if (note.isFlick())
			archetype += "Flick";
		if (!note.friction && !note.isFlick())
			archetype += "Tap";
		archetype += "Note";
		return archetype;
	}

	std::string PySekaiEngine::getHoldNoteArchetype(const Note& note, bool isHead, bool isTail,
	                                                bool isAnchor)
	{
		if (isAnchor)
			return "AnchorNote";

		if (!isHead && !isTail)
			return note.critical ? "CriticalTickNote" : "NormalTickNote";

		std::string archetype = note.critical ? "Critical" : "Normal";
		if (isHead)
			archetype += "Head";
		if (isTail)
			archetype += "Tail";
		if (note.friction)
			archetype += "Trace";
		if (note.isFlick())
			archetype += "Flick";
		if (!note.friction && !note.isFlick())
			archetype += isTail ? "Release" : "Tap";
		archetype += "Note";
		return archetype;
	}

	int PySekaiEngine::toSegmentNumeric(const Note& note, const HoldNote* hold)
	{
		if (hold && hold->isGuide())
		{
			switch (hold->guideColor)
			{
			case GuideColor::Neutral:
				return 101;
			case GuideColor::Red:
				return 102;
			case GuideColor::Green:
				return 103;
			case GuideColor::Blue:
				return 104;
			case GuideColor::Yellow:
				return 105;
			case GuideColor::Purple:
				return 106;
			case GuideColor::Cyan:
				return 107;
			case GuideColor::Black:
				return 108;
			default:
				return 101;
			}
		}
		return (note.critical ? 2 : 1) + (note.dummy ? 50 : 0);
	}

	void PySekaiEngine::insertTransientTickNote(size_t, size_t, bool,
	                                            std::vector<Sonolus::LevelDataEntity>&)
	{
	}

	void PySekaiEngine::estimateAttachEntity(Sonolus::LevelDataEntity&,
	                                         const Sonolus::LevelDataEntity&,
	                                         const Sonolus::LevelDataEntity&)
	{
	}

	int PySekaiEngine::toDirectionNumeric(FlickType flick)
	{
		switch (flick)
		{
		case FlickType::Left:
			return 1;
		case FlickType::Right:
			return 2;
		case FlickType::Down:
			return 3;
		case FlickType::DownLeft:
			return 4;
		case FlickType::DownRight:
			return 5;
		default:
			return 0;
		}
	}

	int PySekaiEngine::toEaseNumeric(EaseType ease)
	{
		switch (ease)
		{
		case EaseType::Linear:
			return 1;
		case EaseType::EaseIn:
			return 2;
		case EaseType::EaseOut:
			return 3;
		case EaseType::EaseInOut:
			return 4;
		case EaseType::EaseOutIn:
			return 5;
		default:
			return 1;
		}
	}

	int PySekaiEngine::toFadeNumeric(FadeType fade)
	{
		switch (fade)
		{
		case FadeType::Out:
			return 0;
		case FadeType::None:
			return 1;
		case FadeType::In:
			return 2;
		default:
			return 0;
		}
	}

	FadeType PySekaiEngine::fromFadeNumeric(int fade)
	{
		switch (fade)
		{
		case 0:
			return FadeType::Out;
		case 1:
			return FadeType::None;
		case 2:
			return FadeType::In;
		default:
			return FadeType::Out;
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
	                                  const std::unordered_map<RefType, size_t>& groupNameMap,
	                                  const std::unordered_map<RefType, id_t>& stageNameMap)
	{
		float beat, lane, size;
		if (!noteEntity.tryGetDataValue("#BEAT", beat) ||
		    !noteEntity.tryGetDataValue("lane", lane) || !noteEntity.tryGetDataValue("size", size))
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

		note.stageID = NO_ID;
		RefType stageRef;
		if (noteEntity.tryGetDataValue("stage", stageRef))
		{
			auto sIt = stageNameMap.find(stageRef);
			if (sIt != stageNameMap.end())
				note.stageID = sIt->second;
		}
		return 1;
	}

	int PySekaiEngine::fromTapNoteEntity(const Sonolus::LevelDataEntity& tapNoteEntity, Note& note,
	                                     const std::unordered_map<RefType, size_t>& groupNameMap,
	                                     const std::unordered_map<RefType, id_t>& stageNameMap,
	                                     NoteType anchorType)
	{
		int status = fromNoteEntity(tapNoteEntity, note, groupNameMap, stageNameMap);
		if (!status)
			return 0;
		int savedLayer = note.layer;
		id_t savedStageID = note.stageID;
		const std::string& arch = tapNoteEntity.archetype;
		bool isCritical = arch.find("Critical") != std::string::npos;
		bool isTrace = arch.find("Trace") != std::string::npos;
		bool isFlick = arch.find("Flick") != std::string::npos;
		int flickDir = 0;
		if (isFlick)
			tapNoteEntity.tryGetDataValue("direction", flickDir);

		if (arch.find("Damage") != std::string::npos)
		{
			note = Note(NoteType::Damage, note.tick, note.lane, note.width);
		}
		else if (arch.find("Tick") != std::string::npos)
		{
			note = Note(NoteType::HoldMid, note.tick, note.lane, note.width);
			note.critical = isCritical;
			note.friction = isTrace;
		}
		else if (arch.find("Head") != std::string::npos)
		{
			note = Note(NoteType::Hold, note.tick, note.lane, note.width);
			note.critical = isCritical;
			note.friction = isTrace;
		}
		else if (arch.find("Tail") != std::string::npos ||
		         arch.find("Release") != std::string::npos)
		{
			note = Note(NoteType::HoldEnd, note.tick, note.lane, note.width);
			note.critical = isCritical;
			note.friction = isTrace;
			if (isFlick)
				note.flick = fromDirectionNumeric(flickDir);
		}
		else if (arch.find("Anchor") != std::string::npos)
		{
			note = Note(anchorType, note.tick, note.lane, note.width);
			note.critical = isCritical;
		}
		else
		{
			note = Note(NoteType::Tap, note.tick, note.lane, note.width);
			note.critical = isCritical;
			note.friction = isTrace;
			if (isFlick)
				note.flick = fromDirectionNumeric(flickDir);
		}

		note.layer = savedLayer;
		note.stageID = savedStageID;

		return status;
	}

	int PySekaiEngine::fromHoldMidEntity(const Sonolus::LevelDataEntity& noteEntity, Note& note,
	                                     const std::unordered_map<RefType, size_t>& groupNameMap,
	                                     const std::unordered_map<RefType, id_t>& stageNameMap,
	                                     NoteType anchorType)
	{
		return fromTapNoteEntity(noteEntity, note, groupNameMap, stageNameMap, anchorType);
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
		case 0:
			return FlickType::Default;
		case 1:
			return FlickType::Left;
		case 2:
			return FlickType::Right;
		case 3:
			return FlickType::Down;
		case 4:
			return FlickType::DownLeft;
		case 5:
			return FlickType::DownRight;
		default:
			return FlickType::None;
		}
	}

	EaseType PySekaiEngine::fromEaseNumeric(int ease)
	{
		switch (ease)
		{
		case 1:
			return EaseType::Linear;
		case 2:
			return EaseType::EaseIn;
		case 3:
			return EaseType::EaseOut;
		case 4:
			return EaseType::EaseInOut;
		case 5:
			return EaseType::EaseOutIn;
		default:
			return EaseType::Linear;
		}
	}

	bool PySekaiEngine::fromSegmentNumeric(int, HoldStep&) { return true; }
}