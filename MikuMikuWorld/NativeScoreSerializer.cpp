#include "NativeScoreSerializer.h"
#include "Localization.h"

namespace MikuMikuWorld
{
	const int MMWS_VERSION = 4;
	const char* MMWS_SIGNATURE = "MMWS";
	const int CC_MMWS_VERSION = 6;
	const char* CC_MMWS_SIGNATURE = "CCMMWS";
	const int UC_MMWS_VERSION = 4;
	const char* UC_MMWS_SIGNATURE = "UCMMWS";

	namespace
	{
		constexpr unsigned int LEGACY_NOTE_CRITICAL = 1 << 0;
		constexpr unsigned int LEGACY_NOTE_FRICTION = 1 << 1;
		constexpr unsigned int LEGACY_NOTE_DUMMY = 1 << 2;

		constexpr unsigned int LEGACY_HOLD_START_HIDDEN = 1 << 0;
		constexpr unsigned int LEGACY_HOLD_END_HIDDEN = 1 << 1;
		constexpr unsigned int LEGACY_HOLD_GUIDE = 1 << 2;
		constexpr unsigned int LEGACY_HOLD_DUMMY = 1 << 3;

		struct LegacyVersion
		{
			int mmwsVersion = 0;
			int ccVersion = 0;

			bool supportJacket() const { return mmwsVersion >= 2; }
			bool supportAddress() const { return mmwsVersion >= 3; }
			bool supportHispeed() const { return mmwsVersion >= 3; }
			bool supportSkillFever() const { return mmwsVersion >= 2; }
			bool supportGuideNote() const { return mmwsVersion >= 4; }
			bool supportDamageNote() const { return ccVersion >= 1; }
			bool supportLaneExtension() const { return ccVersion >= 1; }
			bool supportFadeType() const { return ccVersion >= 2; }
			bool supportGuideColor() const { return ccVersion >= 3; }
			bool supportLayers() const { return ccVersion >= 4; }
			bool supportWaypoints() const { return ccVersion >= 5; }
			bool supportFloatLaneWidth() const { return ccVersion >= 6; }
		};

		Note readLegacyNote(NoteType type, IO::BinaryReader& reader, const LegacyVersion& version)
		{
			Note note(type);
			if (version.supportFloatLaneWidth())
			{
				note.tick = reader.readUInt32();
				note.lane = reader.readSingle();
				note.width = reader.readSingle();
			}
			else
			{
				note.tick = reader.readUInt32();
				note.lane = (float)reader.readUInt32();
				note.width = (float)reader.readUInt32();
			}
			note.tick = std::max(note.tick, 0);

			if (version.supportLayers())
				note.layer = reader.readUInt32();

			if (!note.hasEase())
				note.flick = static_cast<FlickType>(reader.readUInt32());

			unsigned int flags = reader.readUInt32();
			note.critical = (flags & LEGACY_NOTE_CRITICAL) != 0;
			note.friction = (flags & LEGACY_NOTE_FRICTION) != 0;
			note.dummy = (flags & LEGACY_NOTE_DUMMY) != 0;

			if (note.flick >= FlickType::FlickTypeCount)
				note.flick = FlickType::Default;

			return note;
		}

		ScoreMetadata readLegacyMetadata(IO::BinaryReader& reader, const LegacyVersion& version)
		{
			ScoreMetadata metadata;
			metadata.title = reader.readString();
			metadata.author = reader.readString();
			metadata.artist = reader.readString();
			metadata.musicFile = reader.readString();
			metadata.musicOffset = reader.readSingle();
			if (version.supportJacket())
				metadata.jacketFile = reader.readString();
			if (version.supportLaneExtension())
				metadata.laneExtension = reader.readUInt32();
			return metadata;
		}
		enum NoteFlags
		{
			NOTE_CRITICAL = 1 << 0,
			NOTE_FRICTION = 1 << 1,
			NOTE_DUMMY = 1 << 2,
		};

		enum HoldFlags
		{
			HOLD_START_HIDDEN = 1 << 0,
			HOLD_END_HIDDEN = 1 << 1,
			HOLD_GUIDE = 1 << 2,
			HOLD_DUMMY = 1 << 3,
		};

		void writeNote(const Note& note, IO::BinaryWriter& writer)
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
		}

		Note readNote(IO::BinaryReader& reader, NoteType type = NoteType::Tap)
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
			return note;
		}

		void writeMetadata(const ScoreMetadata& metadata, IO::BinaryWriter& writer)
		{
			writer.writeString(metadata.title);
			writer.writeString(metadata.author);
			writer.writeString(metadata.artist);
			writer.writeString(metadata.musicFile);
			writer.writeSingle(metadata.musicOffset);
			writer.writeString(metadata.jacketFile);
			writer.writeInt32(metadata.laneExtension);
			writer.writeInt32(metadata.isExtendedScore ? 1 : 0);
			writer.writeInt32(metadata.baseLifePoint);
		}

		ScoreMetadata readMetadata(IO::BinaryReader& reader, int version = UC_MMWS_VERSION)
		{
			ScoreMetadata metadata;
			metadata.title = reader.readString();
			metadata.author = reader.readString();
			metadata.artist = reader.readString();
			metadata.musicFile = reader.readString();
			metadata.musicOffset = reader.readSingle();
			metadata.jacketFile = reader.readString();
			metadata.laneExtension = reader.readUInt32();
			if (version >= 3)
			{
				metadata.isExtendedScore = reader.readUInt32() != 0;
				metadata.baseLifePoint = reader.readUInt32();
			}
			return metadata;
		}

		void writeScoreEvents(const Score& score, IO::BinaryWriter& writer)
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
				writer.writeInt32(static_cast<int>(skill.level));
			}

			writer.writeInt32(score.fever.startTick);
			writer.writeInt32(score.fever.endTick);
		}

		void readScoreEvents(Score& score, IO::BinaryReader& reader, int version = UC_MMWS_VERSION)
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
				if (version >= 3)
				{
					skill.effect = static_cast<SkillEffect>(reader.readUInt32());
					skill.level = static_cast<uint8_t>(reader.readUInt32());
				}
				score.skills[skill.ID] = skill;
			}

			score.fever.startTick = reader.readUInt32();
			score.fever.endTick = reader.readUInt32();
		}
	}

	void NativeScoreSerializer::serialize(const Score& score, std::string filename)
	{
		IO::BinaryWriter writer(filename);
		if (!writer.isStreamValid())
			return;

		writer.writeString(UC_MMWS_SIGNATURE);
		writer.writeInt32(UC_MMWS_VERSION);

		uint32_t offsetsAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t) * 7);

		uint32_t metadataAddress = writer.getStreamPosition();
		writeMetadata(score.metadata, writer);

		uint32_t eventsAddress = writer.getStreamPosition();
		writeScoreEvents(score, writer);

		uint32_t tapsAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t));


		int noteCount = 0;
		for (const auto& [_, note] : score.notes)
		{
			if (note.getType() != NoteType::Tap)
				continue;
			writeNote(note, writer);
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
			writeNote(startNote, writer);
			writer.writeInt32(static_cast<int>(hold.start.ease));
			writer.writeInt32(static_cast<int>(hold.fadeType));
			writer.writeInt32(static_cast<int>(hold.guideColor));

			writer.writeInt32((int)hold.steps.size());
			for (const auto& step : hold.steps)
			{
				const Note& mid = score.notes.at(step.ID);
				writeNote(mid, writer);
				writer.writeInt32(static_cast<int>(step.type));
				writer.writeInt32(static_cast<int>(step.ease));
			}

			const Note& endNote = score.notes.at(hold.end);
			writeNote(endNote, writer);
		}

		uint32_t damagesAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t));

		int damageCount = 0;
		for (const auto& [_, note] : score.notes)
		{
			if (note.getType() != NoteType::Damage)
				continue;
			writeNote(note, writer);
			++damageCount;
		}

		uint32_t layersAddress = writer.getStreamPosition();

		writer.seek(damagesAddress);
		writer.writeInt32(damageCount);
		writer.seek(layersAddress);

		writer.writeInt32((int)score.layers.size());
		for (const auto& layer : score.layers)
			writer.writeString(layer.name);

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
		writer.writeInt32(tapsAddress);
		writer.writeInt32(holdsAddress);
		writer.writeInt32(damagesAddress);
		writer.writeInt32(layersAddress);
		writer.writeInt32(waypointsAddress);

		writer.flush();
		writer.close();
	}

	static Score deserializeLegacy(IO::BinaryReader& reader, const LegacyVersion& version)
	{
		Score score;

		uint32_t metadataAddress{}, eventsAddress{}, tapsAddress{}, holdsAddress{};
		uint32_t damagesAddress{}, layersAddress{}, waypointsAddress{};

		if (version.supportAddress())
		{
			metadataAddress = reader.readUInt32();
			eventsAddress = reader.readUInt32();
			tapsAddress = reader.readUInt32();
			holdsAddress = reader.readUInt32();
			if (version.supportDamageNote())
				damagesAddress = reader.readUInt32();
			if (version.supportLayers())
				layersAddress = reader.readUInt32();
			if (version.supportWaypoints())
				waypointsAddress = reader.readUInt32();
			reader.seek(metadataAddress);
		}

		score.metadata = readLegacyMetadata(reader, version);

		if (version.supportAddress())
			reader.seek(eventsAddress);

		int timeSignatureCount = reader.readUInt32();
		if (timeSignatureCount)
			score.timeSignatures.clear();
		for (int i = 0; i < timeSignatureCount; ++i)
		{
			int measure = reader.readUInt32();
			int numerator = reader.readUInt32();
			int denominator = reader.readUInt32();
			score.timeSignatures[measure] = { measure, numerator, denominator };
		}

		int tempoCount = reader.readUInt32();
		if (tempoCount)
			score.tempoChanges.clear();
		for (int i = 0; i < tempoCount; ++i)
		{
			int tick = reader.readUInt32();
			float bpm = reader.readSingle();
			score.tempoChanges.push_back({ tick, bpm });
		}

		if (version.supportHispeed())
		{
			int hsCount = reader.readUInt32();
			if (hsCount)
				score.hiSpeedChanges.clear();
			for (int i = 0; i < hsCount; ++i)
			{
				int tick = reader.readUInt32();
				float speed = reader.readSingle();
				id_t id = getNextHiSpeedID();
				score.hiSpeedChanges[id] = {
					id, tick, speed, 0, 0.0f, HiSpeedEaseType::None, false
				};
			}
		}

		if (version.supportSkillFever())
		{
			int skillCount = reader.readUInt32();
			for (int i = 0; i < skillCount; ++i)
			{
				int tick = reader.readUInt32();
				id_t id = getNextSkillID();
				score.skills[id] = { id, tick };
			}
			score.fever.startTick = reader.readUInt32();
			score.fever.endTick = reader.readUInt32();
		}

		if (version.supportAddress())
			reader.seek(tapsAddress);

		int noteCount = reader.readUInt32();
		score.notes.reserve(noteCount);
		for (int i = 0; i < noteCount; ++i)
		{
			Note note = readLegacyNote(NoteType::Tap, reader, version);
			note.ID = Note::getNextID();
			score.notes[note.ID] = note;
		}

		if (version.supportAddress())
			reader.seek(holdsAddress);

		int holdCount = reader.readUInt32();
		score.holdNotes.reserve(holdCount);
		for (int i = 0; i < holdCount; ++i)
		{
			HoldNote hold;
			unsigned int flags = version.supportGuideNote() ? reader.readUInt32() : 0;

			Note start = readLegacyNote(NoteType::Hold, reader, version);
			start.ID = Note::getNextID();

			hold.start.ease = static_cast<EaseType>(reader.readUInt32());
			if (hold.start.ease >= EaseType::EaseTypeCount)
				hold.start.ease = EaseType::Linear;
			hold.start.type = HoldStepType::Normal;
			hold.start.ID = start.ID;

			bool isGuide = (flags & LEGACY_HOLD_GUIDE) != 0;
			if (isGuide)
				hold.startType = hold.endType = HoldNoteType::Guide;
			else if (flags & LEGACY_HOLD_START_HIDDEN)
				hold.startType = HoldNoteType::Hidden;

			if (flags & LEGACY_HOLD_DUMMY)
				hold.dummy = true;

			if (version.supportFadeType())
				hold.fadeType = static_cast<FadeType>(reader.readUInt32());
			else
				hold.fadeType = FadeType::Out;

			if (version.supportGuideColor())
				hold.guideColor = static_cast<GuideColor>(reader.readUInt32());
			else
				hold.guideColor = start.critical ? GuideColor::Yellow : GuideColor::Green;

			score.notes[start.ID] = start;

			int stepCount = reader.readUInt32();
			hold.steps.reserve(stepCount);
			for (int j = 0; j < stepCount; ++j)
			{
				Note mid = readLegacyNote(NoteType::HoldMid, reader, version);
				mid.ID = Note::getNextID();
				mid.parentID = start.ID;

				HoldStep step{};
				step.ID = mid.ID;
				step.type = static_cast<HoldStepType>(reader.readUInt32());
				step.ease = static_cast<EaseType>(reader.readUInt32());
				if (step.ease >= EaseType::EaseTypeCount)
					step.ease = EaseType::Linear;

				score.notes[mid.ID] = mid;
				hold.steps.push_back(step);
			}

			Note end = readLegacyNote(NoteType::HoldEnd, reader, version);
			end.ID = Note::getNextID();
			end.parentID = start.ID;
			if (!isGuide && (flags & LEGACY_HOLD_END_HIDDEN))
				hold.endType = HoldNoteType::Hidden;

			score.notes[end.ID] = end;
			hold.end = end.ID;

			score.holdNotes[start.ID] = hold;
		}

		if (version.supportDamageNote())
		{
			reader.seek(damagesAddress);
			int damageCount = reader.readUInt32();
			for (int i = 0; i < damageCount; ++i)
			{
				Note note = readLegacyNote(NoteType::Damage, reader, version);
				note.ID = Note::getNextID();
				score.notes[note.ID] = note;
			}
		}

		if (version.supportLayers())
		{
			score.layers.clear();
			reader.seek(layersAddress);
			int layerCount = reader.readUInt32();
			for (int i = 0; i < layerCount; ++i)
				score.layers.push_back({ reader.readString() });
		}

		if (version.supportWaypoints())
		{
			score.waypoints.clear();
			reader.seek(waypointsAddress);
			int waypointCount = reader.readUInt32();
			for (int i = 0; i < waypointCount; ++i)
			{
				std::string name = reader.readString();
				int tick = reader.readUInt32();
				score.waypoints.push_back({ name, tick });
			}
		}

		return score;
	}

	Score NativeScoreSerializer::deserialize(std::string filename)
	{
		Score score;
		IO::BinaryReader reader(filename);
		if (!reader.isStreamValid())
			return score;

		std::string signature = reader.readString();

		if (signature == MMWS_SIGNATURE)
		{
			LegacyVersion version{ (int)reader.readUInt32(), 0 };
			Score score = deserializeLegacy(reader, version);
			reader.close();
			return score;
		}

		if (signature == CC_MMWS_SIGNATURE)
		{
			LegacyVersion version{};
			version.ccVersion = std::max((int)reader.readUInt16(), 1);
			version.mmwsVersion = (int)reader.readUInt16();
			Score score = deserializeLegacy(reader, version);
			reader.close();
			return score;
		}

		if (signature != UC_MMWS_SIGNATURE)
		{
			reader.close();
			throw std::runtime_error("Invalid MMWS file. Unrecognized signature");
		}

		int version = reader.readUInt32();
		(void)version;

		uint32_t metadataAddress = reader.readUInt32();
		uint32_t eventsAddress = reader.readUInt32();
		uint32_t tapsAddress = reader.readUInt32();
		uint32_t holdsAddress = reader.readUInt32();
		uint32_t damagesAddress = reader.readUInt32();
		uint32_t layersAddress = reader.readUInt32();
		uint32_t waypointsAddress = reader.readUInt32();

		reader.seek(metadataAddress);
		score.metadata = readMetadata(reader);

		reader.seek(eventsAddress);
		readScoreEvents(score, reader);

		reader.seek(tapsAddress);
		int noteCount = reader.readUInt32();
		id_t nextID = 0;
		for (int i = 0; i < noteCount; ++i)
		{
			Note note = readNote(reader, NoteType::Tap);
			note.ID = nextID++;
			score.notes[note.ID] = note;
		}

		reader.seek(holdsAddress);
		int holdCount = reader.readUInt32();
		id_t nextHoldID = 0;
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

			Note startNote = readNote(reader, NoteType::Hold);
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
				Note mid = readNote(reader, NoteType::HoldMid);
				mid.ID = nextID++;
				mid.parentID = startNote.ID;
				score.notes[mid.ID] = mid;

				HoldStep step{};
				step.ID = mid.ID;
				step.type = static_cast<HoldStepType>(reader.readUInt32());
				step.ease = static_cast<EaseType>(reader.readUInt32());
				hold.steps.push_back(step);
			}

			Note endNote = readNote(reader, NoteType::HoldEnd);
			endNote.ID = nextID++;
			endNote.parentID = startNote.ID;
			score.notes[endNote.ID] = endNote;

			hold.end = endNote.ID;
			score.holdNotes[hold.start.ID] = hold;
			++nextHoldID;
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
			Note note = readNote(reader, NoteType::Damage);
			note.ID = nextID++;
			score.notes[note.ID] = note;
		}

		reader.seek(layersAddress);
		int layerCount = reader.readUInt32();
		score.layers.clear();
		for (int i = 0; i < layerCount; ++i)
			score.layers.push_back({ reader.readString() });

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

	bool NativeScoreSerializer::canSerialize(const Score&) { return true; }
}