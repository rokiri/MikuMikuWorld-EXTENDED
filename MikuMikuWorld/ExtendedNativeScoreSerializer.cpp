#include "ExtendedNativeScoreSerializer.h"
#include "DynamicStage.h"

namespace MikuMikuWorld
{
	static const char* EX_MMWS_SIGNATURE = "UCEXMMWS";
	static const int EX_MMWS_VERSION = 2;

	namespace
	{
		enum NoteFlagsEx
		{
			NOTE_CRITICAL = 1 << 0,
			NOTE_FRICTION = 1 << 1,
			NOTE_DUMMY = 1 << 2,
		};

		enum HoldFlagsEx
		{
			HOLD_START_HIDDEN = 1 << 0,
			HOLD_END_HIDDEN = 1 << 1,
			HOLD_GUIDE = 1 << 2,
			HOLD_DUMMY = 1 << 3,
		};

		void writeNoteEx(const Note& note, IO::BinaryWriter& writer,
		                 const std::unordered_map<id_t, int>& stageIndexOf)
		{
			writer.writeInt32(note.tick);
			writer.writeSingle(note.lane);
			writer.writeSingle(note.width);
			writer.writeInt32(note.layer);
			if (!note.hasEase())
				writer.writeInt32(static_cast<int>(note.flick));
			unsigned int flags{};
			if (note.critical)
				flags |= NOTE_CRITICAL;
			if (note.friction)
				flags |= NOTE_FRICTION;
			if (note.dummy)
				flags |= NOTE_DUMMY;
			writer.writeInt32(flags);

			auto it = stageIndexOf.find(note.stageID);
			writer.writeInt32(it != stageIndexOf.end() ? it->second : -1);
		}

		Note readNoteEx(IO::BinaryReader& reader, const std::vector<id_t>& stageIndexToID,
		                NoteType type = NoteType::Tap)
		{
			Note note(type);
			note.tick = reader.readUInt32();
			note.lane = reader.readSingle();
			note.width = reader.readSingle();
			note.layer = reader.readUInt32();
			if (!note.hasEase())
			{
				note.flick = static_cast<FlickType>(reader.readUInt32());
				if (note.flick >= FlickType::FlickTypeCount)
					note.flick = FlickType::None;
			}
			unsigned int flags = reader.readUInt32();
			note.critical = (bool)(flags & NOTE_CRITICAL);
			note.friction = (bool)(flags & NOTE_FRICTION);
			note.dummy = (bool)(flags & NOTE_DUMMY);

			int stageIdx = reader.readInt32();
			note.stageID = (stageIdx >= 0 && stageIdx < (int)stageIndexToID.size())
			                   ? stageIndexToID[stageIdx]
			                   : NO_ID;
			return note;
		}

		void writeMetadataEx(const ScoreMetadata& metadata, IO::BinaryWriter& writer)
		{
			writer.writeString(metadata.title);
			writer.writeString(metadata.author);
			writer.writeString(metadata.artist);
			writer.writeString(metadata.musicFile);
			writer.writeSingle(metadata.musicOffset);
			writer.writeString(metadata.jacketFile);
			writer.writeInt32(metadata.laneExtension);
			writer.writeInt32(metadata.baseLifePoint);
		}

		ScoreMetadata readMetadataEx(IO::BinaryReader& reader)
		{
			ScoreMetadata metadata;
			metadata.title = reader.readString();
			metadata.author = reader.readString();
			metadata.artist = reader.readString();
			metadata.musicFile = reader.readString();
			metadata.musicOffset = reader.readSingle();
			metadata.jacketFile = reader.readString();
			metadata.laneExtension = reader.readUInt32();
			metadata.baseLifePoint = reader.readUInt32();
			return metadata;
		}

		void writeScoreEventsEx(const Score& score, IO::BinaryWriter& writer)
		{
			writer.writeInt32((int)score.timeSignatures.size());
			for (const auto& [_, ts] : score.timeSignatures)
			{
				writer.writeInt32(ts.measure);
				writer.writeInt32(ts.numerator);
				writer.writeInt32(ts.denominator);
			}

			writer.writeInt32((int)score.tempoChanges.size());
			for (const auto& tempo : score.tempoChanges)
			{
				writer.writeInt32(tempo.tick);
				writer.writeSingle(tempo.bpm);
			}

			writer.writeInt32((int)score.hiSpeedChanges.size());
			for (const auto& [_, hs] : score.hiSpeedChanges)
			{
				writer.writeInt32(hs.tick);
				writer.writeSingle(hs.speed);
				writer.writeInt32(hs.layer);
				writer.writeSingle(hs.skips);
				writer.writeInt16(static_cast<int>(hs.ease));
				writer.writeInt16(hs.hideNotes ? 1 : 0);
			}

			writer.writeInt32((int)score.skills.size());
			for (const auto& [_, skill] : score.skills)
			{
				writer.writeInt32(skill.tick);
				writer.writeInt32(static_cast<int>(skill.effect));
				writer.writeInt32(skill.level);
			}

			writer.writeInt32(score.fever.startTick);
			writer.writeInt32(score.fever.endTick);
		}

		void readScoreEventsEx(Score& score, IO::BinaryReader& reader)
		{
			int tsCount = reader.readUInt32();
			score.timeSignatures.clear();
			for (int i = 0; i < tsCount; ++i)
			{
				int measure = reader.readUInt32();
				int num = reader.readUInt32();
				int den = reader.readUInt32();
				score.timeSignatures[measure] = { measure, num, den };
			}

			int tempoCount = reader.readUInt32();
			score.tempoChanges.clear();
			for (int i = 0; i < tempoCount; ++i)
			{
				int tick = reader.readUInt32();
				float bpm = reader.readSingle();
				score.tempoChanges.push_back({ tick, bpm });
			}

			int hsCount = reader.readUInt32();
			score.hiSpeedChanges.clear();
			id_t nextHsID = 0;
			for (int i = 0; i < hsCount; ++i)
			{
				HiSpeedChange hs;
				hs.ID = nextHsID++;
				hs.tick = reader.readUInt32();
				hs.speed = reader.readSingle();
				hs.layer = reader.readUInt32();
				hs.skips = reader.readSingle();
				hs.ease = static_cast<HiSpeedEaseType>(reader.readUInt16());
				hs.hideNotes = reader.readUInt16() != 0;
				score.hiSpeedChanges[hs.ID] = hs;
			}

			int skillCount = reader.readUInt32();
			score.skills.clear();
			id_t nextSkillID = 0;
			for (int i = 0; i < skillCount; ++i)
			{
				SkillTrigger skill;
				skill.ID = nextSkillID++;
				skill.tick = reader.readUInt32();
				skill.effect = static_cast<SkillEffect>(reader.readUInt32());
				skill.level = reader.readUInt32();
				score.skills[skill.ID] = skill;
			}

			score.fever.startTick = reader.readUInt32();
			score.fever.endTick = reader.readUInt32();
		}
	}

	void ExtendedNativeScoreSerializer::serialize(const Score& score, std::string filename)
	{
		IO::BinaryWriter writer(filename);
		if (!writer.isStreamValid())
			return;

		writer.writeString(EX_MMWS_SIGNATURE);
		writer.writeInt32(EX_MMWS_VERSION);

		uint32_t offsetsAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t) * 8);

		uint32_t metadataAddress = writer.getStreamPosition();
		writeMetadataEx(score.metadata, writer);

		uint32_t eventsAddress = writer.getStreamPosition();
		writeScoreEventsEx(score, writer);

		uint32_t stagesAddress = writer.getStreamPosition();
		std::unordered_map<id_t, int> stageIndexOf;
		writer.writeInt32((int)score.stageOrder.size());
		{
			int idx = 0;
			for (id_t id : score.stageOrder)
			{
				auto it = score.stages.find(id);
				if (it == score.stages.end())
					continue;
				const Stage& stage = it->second;
				stageIndexOf[id] = idx++;
				writer.writeString(stage.editorName);
				writer.writeInt32(stage.fromStart ? 1 : 0);
				writer.writeInt32(stage.untilEnd ? 1 : 0);
				writer.writeInt32(stage.generateSimLinesIsolated ? 1 : 0);
			}
		}

		writer.writeInt32((int)score.cameraChanges.size());
		for (const auto& [_, camera] : score.cameraChanges)
		{
			writer.writeInt32(camera.tick);
			writer.writeSingle(camera.left);
			writer.writeSingle(camera.size);
			writer.writeSingle(camera.zoom);
			writer.writeSingle(camera.zoomTargetLane);
			writer.writeSingle(camera.zoomTargetY);
			writer.writeInt32(static_cast<int>(camera.zoomVerticalAlign));
			writer.writeSingle(camera.rotate);
			writer.writeSingle(camera.stageTilt);
			writer.writeInt32(static_cast<int>(camera.ease));
		}

		auto writeStageRefIndex = [&](id_t stageID)
		{
			auto it = stageIndexOf.find(stageID);
			writer.writeInt32(it != stageIndexOf.end() ? it->second : -1);
		};

		writer.writeInt32((int)score.stageMaskChanges.size());
		for (const auto& [_, mask] : score.stageMaskChanges)
		{
			writeStageRefIndex(mask.stageID);
			writer.writeInt32(mask.tick);
			writer.writeSingle(mask.left);
			writer.writeSingle(mask.size);
			writer.writeInt32(static_cast<int>(mask.ease));
		}

		writer.writeInt32((int)score.stagePivotChanges.size());
		for (const auto& [_, pivot] : score.stagePivotChanges)
		{
			writeStageRefIndex(pivot.stageID);
			writer.writeInt32(pivot.tick);
			writer.writeSingle(pivot.lane);
			writer.writeInt32(pivot.divisionSize);
			writer.writeInt32(pivot.divisionParityOdd ? 1 : 0);
			writer.writeSingle(pivot.yOffset);
			writer.writeSingle(pivot.yOffsetBeat);
			writer.writeInt32(static_cast<int>(pivot.ease));
		}

		writer.writeInt32((int)score.stageStyleChanges.size());
		for (const auto& [_, style] : score.stageStyleChanges)
		{
			writeStageRefIndex(style.stageID);
			writer.writeInt32(style.tick);
			writer.writeInt32(static_cast<int>(style.judgeLineColor));
			writer.writeInt32(static_cast<int>(style.leftBorderStyle));
			writer.writeInt32(static_cast<int>(style.rightBorderStyle));
			writer.writeSingle(style.alpha);
			writer.writeSingle(style.laneAlpha);
			writer.writeSingle(style.judgeLineAlpha);
			writer.writeInt32(static_cast<int>(style.ease));
		}

		writer.writeInt32((int)score.stageTransformChanges.size());
		for (const auto& [_, transform] : score.stageTransformChanges)
		{
			writeStageRefIndex(transform.stageID);
			writer.writeInt32(transform.tick);
			writer.writeSingle(transform.rotate);
			writer.writeSingle(transform.xLaneTranslate);
			writer.writeSingle(transform.yLaneTranslate);
			writer.writeInt32(static_cast<int>(transform.anchor));
			writer.writeInt32(static_cast<int>(transform.ease));
		}

		uint32_t tapsAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t));

		int noteCount = 0;
		for (const auto& [_, note] : score.notes)
		{
			if (note.getType() != NoteType::Tap)
				continue;
			writeNoteEx(note, writer, stageIndexOf);
			++noteCount;
		}

		uint32_t holdsAddress = writer.getStreamPosition();
		writer.seek(tapsAddress);
		writer.writeInt32(noteCount);
		writer.seek(holdsAddress);

		writer.writeInt32((int)score.holdNotes.size());
		for (const auto& [_, hold] : score.holdNotes)
		{
			unsigned int flags{};
			if (hold.startType == HoldNoteType::Guide)
				flags |= HOLD_GUIDE;
			if (hold.startType == HoldNoteType::Hidden)
				flags |= HOLD_START_HIDDEN;
			if (hold.endType == HoldNoteType::Hidden)
				flags |= HOLD_END_HIDDEN;
			if (hold.dummy)
				flags |= HOLD_DUMMY;
			writer.writeInt32(flags);

			const Note& startNote = score.notes.at(hold.start.ID);
			writeNoteEx(startNote, writer, stageIndexOf);
			writer.writeInt32(static_cast<int>(hold.start.ease));
			writer.writeInt32(static_cast<int>(hold.fadeType));
			writer.writeInt32(static_cast<int>(hold.guideColor));

			writer.writeInt32((int)hold.steps.size());
			for (const auto& step : hold.steps)
			{
				const Note& mid = score.notes.at(step.ID);
				writeNoteEx(mid, writer, stageIndexOf);
				writer.writeInt32(static_cast<int>(step.type));
				writer.writeInt32(static_cast<int>(step.ease));
			}

			const Note& endNote = score.notes.at(hold.end);
			writeNoteEx(endNote, writer, stageIndexOf);
		}

		uint32_t damagesAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t));

		int damageCount = 0;
		for (const auto& [_, note] : score.notes)
		{
			if (note.getType() != NoteType::Damage)
				continue;
			writeNoteEx(note, writer, stageIndexOf);
			++damageCount;
		}

		uint32_t layersAddress = writer.getStreamPosition();
		writer.seek(damagesAddress);
		writer.writeInt32(damageCount);
		writer.seek(layersAddress);

		writer.writeInt32((int)score.layers.size());
		for (const auto& layer : score.layers)
		{
			writer.writeString(layer.name);
			writer.writeSingle(layer.forceNoteSpeed);
		}

		uint32_t waypointsAddress = writer.getStreamPosition();
		writer.writeInt32((int)score.waypoints.size());
		for (const auto& wp : score.waypoints)
		{
			writer.writeString(wp.name);
			writer.writeInt32(wp.tick);
		}

		writer.seek(offsetsAddress);
		writer.writeInt32(metadataAddress);
		writer.writeInt32(eventsAddress);
		writer.writeInt32(stagesAddress);
		writer.writeInt32(tapsAddress);
		writer.writeInt32(holdsAddress);
		writer.writeInt32(damagesAddress);
		writer.writeInt32(layersAddress);
		writer.writeInt32(waypointsAddress);

		writer.flush();
		writer.close();
	}

	Score ExtendedNativeScoreSerializer::deserialize(std::string filename)
	{
		Score score;
		IO::BinaryReader reader(filename);
		if (!reader.isStreamValid())
			return score;

		std::string signature = reader.readString();
		if (signature != EX_MMWS_SIGNATURE)
		{
			reader.close();
			throw std::runtime_error("Invalid MMWPPS file. Unrecognized signature");
		}

		int version = reader.readUInt32();

		uint32_t metadataAddress = reader.readUInt32();
		uint32_t eventsAddress = reader.readUInt32();
		uint32_t stagesAddress = reader.readUInt32();
		uint32_t tapsAddress = reader.readUInt32();
		uint32_t holdsAddress = reader.readUInt32();
		uint32_t damagesAddress = reader.readUInt32();
		uint32_t layersAddress = reader.readUInt32();
		uint32_t waypointsAddress = reader.readUInt32();

		reader.seek(metadataAddress);
		score.metadata = readMetadataEx(reader);

		reader.seek(eventsAddress);
		readScoreEventsEx(score, reader);

		reader.seek(stagesAddress);
		std::vector<id_t> stageIndexToID;
		int stageCount = reader.readUInt32();
		score.stages.clear();
		score.stageOrder.clear();
		for (int i = 0; i < stageCount; ++i)
		{
			Stage stage;
			stage.ID = getNextStageID();
			stage.editorName = reader.readString();
			stage.fromStart = reader.readUInt32() != 0;
			stage.untilEnd = reader.readUInt32() != 0;
			stage.generateSimLinesIsolated = reader.readUInt32() != 0;
			score.stages[stage.ID] = stage;
			score.stageOrder.push_back(stage.ID);
			stageIndexToID.push_back(stage.ID);
		}

		int cameraCount = reader.readUInt32();
		score.cameraChanges.clear();
		for (int i = 0; i < cameraCount; ++i)
		{
			CameraChangeEvent camera;
			camera.ID = getNextCameraChangeID();
			camera.tick = reader.readUInt32();
			camera.left = reader.readSingle();
			camera.size = reader.readSingle();
			camera.zoom = reader.readSingle();
			camera.zoomTargetLane = reader.readSingle();
			camera.zoomTargetY = reader.readSingle();
			camera.zoomVerticalAlign = static_cast<StageZoomVerticalAlign>(reader.readUInt32());
			camera.rotate = reader.readSingle();
			camera.stageTilt = reader.readSingle();
			camera.ease = static_cast<EaseType>(reader.readUInt32());
			score.cameraChanges[camera.ID] = camera;
		}

		auto resolveStageIndex = [&](int idx) -> id_t
		{ return (idx >= 0 && idx < (int)stageIndexToID.size()) ? stageIndexToID[idx] : NO_ID; };

		int maskCount = reader.readUInt32();
		score.stageMaskChanges.clear();
		for (int i = 0; i < maskCount; ++i)
		{
			StageMaskChangeEvent mask;
			mask.ID = getNextStageMaskChangeID();
			mask.stageID = resolveStageIndex(reader.readInt32());
			mask.tick = reader.readUInt32();
			mask.left = reader.readSingle();
			mask.size = reader.readSingle();
			mask.ease = static_cast<EaseType>(reader.readUInt32());
			score.stageMaskChanges[mask.ID] = mask;
		}

		int pivotCount = reader.readUInt32();
		score.stagePivotChanges.clear();
		for (int i = 0; i < pivotCount; ++i)
		{
			StagePivotChangeEvent pivot;
			pivot.ID = getNextStagePivotChangeID();
			pivot.stageID = resolveStageIndex(reader.readInt32());
			pivot.tick = reader.readUInt32();
			pivot.lane = reader.readSingle();
			pivot.divisionSize = reader.readUInt32();
			pivot.divisionParityOdd = reader.readUInt32() != 0;
			pivot.yOffset = reader.readSingle();
			pivot.yOffsetBeat = reader.readSingle();
			pivot.ease = static_cast<EaseType>(reader.readUInt32());
			score.stagePivotChanges[pivot.ID] = pivot;
		}

		int styleCount = reader.readUInt32();
		score.stageStyleChanges.clear();
		for (int i = 0; i < styleCount; ++i)
		{
			StageStyleChangeEvent style;
			style.ID = getNextStageStyleChangeID();
			style.stageID = resolveStageIndex(reader.readInt32());
			style.tick = reader.readUInt32();
			style.judgeLineColor = static_cast<GuideColor>(reader.readUInt32());
			style.leftBorderStyle = static_cast<StageBorderStyle>(reader.readUInt32());
			style.rightBorderStyle = static_cast<StageBorderStyle>(reader.readUInt32());
			style.alpha = reader.readSingle();
			style.laneAlpha = reader.readSingle();
			style.judgeLineAlpha = reader.readSingle();
			style.ease = static_cast<EaseType>(reader.readUInt32());
			score.stageStyleChanges[style.ID] = style;
		}

		int transformCount = reader.readUInt32();
		score.stageTransformChanges.clear();
		for (int i = 0; i < transformCount; ++i)
		{
			StageTransformEvent transform;
			transform.ID = getNextStageTransformChangeID();
			transform.stageID = resolveStageIndex(reader.readInt32());
			transform.tick = reader.readUInt32();
			transform.rotate = reader.readSingle();
			transform.xLaneTranslate = reader.readSingle();
			transform.yLaneTranslate = reader.readSingle();
			transform.anchor = static_cast<StageTransformAnchor>(reader.readUInt32());
			transform.ease = static_cast<EaseType>(reader.readUInt32());
			score.stageTransformChanges[transform.ID] = transform;
		}

		reader.seek(tapsAddress);
		int noteCount = reader.readUInt32();
		id_t nextID = 0;
		for (int i = 0; i < noteCount; ++i)
		{
			Note note = readNoteEx(reader, stageIndexToID, NoteType::Tap);
			note.ID = nextID++;
			score.notes[note.ID] = note;
		}

		reader.seek(holdsAddress);
		int holdCount = reader.readUInt32();
		for (int i = 0; i < holdCount; ++i)
		{
			HoldNote hold;
			unsigned int flags = reader.readUInt32();

			if (flags & HOLD_GUIDE)
				hold.startType = hold.endType = HoldNoteType::Guide;
			if (flags & HOLD_START_HIDDEN)
				hold.startType = HoldNoteType::Hidden;
			if (flags & HOLD_END_HIDDEN)
				hold.endType = HoldNoteType::Hidden;
			if (flags & HOLD_DUMMY)
				hold.dummy = true;

			Note startNote = readNoteEx(reader, stageIndexToID, NoteType::Hold);
			startNote.ID = nextID++;
			hold.start.ID = startNote.ID;
			hold.start.ease = static_cast<EaseType>(reader.readUInt32());
			hold.start.type = HoldStepType::Normal;
			hold.fadeType = static_cast<FadeType>(reader.readUInt32());
			hold.guideColor = static_cast<GuideColor>(reader.readUInt32());
			score.notes[startNote.ID] = startNote;

			int stepCount = reader.readUInt32();
			for (int j = 0; j < stepCount; ++j)
			{
				Note mid = readNoteEx(reader, stageIndexToID, NoteType::HoldMid);
				mid.ID = nextID++;
				HoldStep step;
				step.ID = mid.ID;
				step.type = static_cast<HoldStepType>(reader.readUInt32());
				step.ease = static_cast<EaseType>(reader.readUInt32());
				hold.steps.push_back(step);
				mid.parentID = startNote.ID;
				score.notes[mid.ID] = mid;
			}

			Note endNote = readNoteEx(reader, stageIndexToID, NoteType::HoldEnd);
			endNote.ID = nextID++;
			hold.end = endNote.ID;
			endNote.parentID = startNote.ID;
			score.notes[endNote.ID] = endNote;

			score.holdNotes[hold.start.ID] = hold;
		}

		for (auto& [holdID, hold] : score.holdNotes)
		{
			score.notes[hold.start.ID].parentID = hold.start.ID;
			for (const auto& step : hold.steps)
				score.notes[step.ID].parentID = hold.start.ID;
			score.notes[hold.end].parentID = hold.start.ID;
		}

		reader.seek(damagesAddress);
		int damageCount = reader.readUInt32();
		for (int i = 0; i < damageCount; ++i)
		{
			Note note = readNoteEx(reader, stageIndexToID, NoteType::Damage);
			note.ID = nextID++;
			score.notes[note.ID] = note;
		}

		reader.seek(layersAddress);
		int layerCount = reader.readUInt32();
		score.layers.clear();
		for (int i = 0; i < layerCount; ++i)
		{
			std::string name = reader.readString();
			float forceNoteSpeed = version >= 2 ? reader.readSingle() : 0.0f;
			if (forceNoteSpeed < 1.0f || forceNoteSpeed > 12.0f)
				forceNoteSpeed = 0.0f;
			score.layers.push_back({ name, forceNoteSpeed });
		}

		reader.seek(waypointsAddress);
		int waypointCount = reader.readUInt32();
		score.waypoints.clear();
		for (int i = 0; i < waypointCount; ++i)
		{
			std::string name = reader.readString();
			int tick = reader.readUInt32();
			score.waypoints.push_back({ name, tick });
		}

		reader.close();
		return score;
	}

	bool ExtendedNativeScoreSerializer::canSerialize(const Score&) { return true; }
}