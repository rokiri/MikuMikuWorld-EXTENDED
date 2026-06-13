#include "NativeScoreSerializer.h"
#include "Localization.h"

namespace MikuMikuWorld
{
	const int MMWS_VERSION = 1;
	const char* UC_MMWS_SIGNATURE = "UCMMWS";

	namespace
	{
		void writeNote(const Note& note, IO::BinaryWriter& writer)
		{
			writer.writeInt32(note.tick);
			writer.writeSingle(note.lane);
			writer.writeSingle(note.width);
			writer.writeInt32(note.layer);
			writer.writeInt32(static_cast<int>(note.getType()));
			writer.writeInt32(note.critical ? 1 : 0);
			writer.writeInt32(note.friction ? 1 : 0);
			writer.writeInt32(note.dummy ? 1 : 0);
			writer.writeInt32(static_cast<int>(note.flick));
		}

		Note readNote(IO::BinaryReader& reader)
		{
			Note note;
			note.tick = reader.readUInt32();
			note.lane = reader.readSingle();
			note.width = reader.readSingle();
			note.layer = reader.readUInt32();
			static_cast<void>(reader.readUInt32()); // type (unused, inferred from context)
			note.critical = reader.readUInt32() != 0;
			note.friction = reader.readUInt32() != 0;
			note.dummy = reader.readUInt32() != 0;
			note.flick = static_cast<FlickType>(reader.readUInt32());
			if (note.flick >= FlickType::FlickTypeCount)
				note.flick = FlickType::None;
			return note;
		}

		void writeMetadata(const ScoreMetadata& metadata, IO::BinaryWriter& writer)
		{
			writer.writeString(metadata.title);
			writer.writeString(metadata.author);
			writer.writeString(metadata.artist);
			writer.writeString(metadata.musicFile);
			writer.writeString(metadata.jacketFile);
			writer.writeSingle(metadata.musicOffset);
			writer.writeInt32(metadata.laneExtension);
		}

		ScoreMetadata readMetadata(IO::BinaryReader& reader)
		{
			ScoreMetadata metadata;
			metadata.title = reader.readString();
			metadata.author = reader.readString();
			metadata.artist = reader.readString();
			metadata.musicFile = reader.readString();
			metadata.jacketFile = reader.readString();
			metadata.musicOffset = reader.readSingle();
			metadata.laneExtension = reader.readUInt32();
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
				writer.writeInt32(static_cast<int>(hs.ease));
				writer.writeInt32(hs.hideNotes ? 1 : 0);
			}

			writer.writeInt32((int)score.skills.size());
			for (const auto& [_, skill] : score.skills)
				writer.writeInt32(skill.tick);

			writer.writeInt32(score.fever.startTick);
			writer.writeInt32(score.fever.endTick);
		}

		void readScoreEvents(Score& score, IO::BinaryReader& reader)
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
				hs.ease = static_cast<HiSpeedEaseType>(reader.readUInt32());
				hs.hideNotes = reader.readUInt32() != 0;
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
		writer.writeInt32(MMWS_VERSION);

		uint32_t offsetsAddress = writer.getStreamPosition();
		writer.writeNull(sizeof(uint32_t) * 4);

		uint32_t metadataAddress = writer.getStreamPosition();
		writeMetadata(score.metadata, writer);

		uint32_t eventsAddress = writer.getStreamPosition();
		writeScoreEvents(score, writer);

		uint32_t layersAddress = writer.getStreamPosition();
		writer.writeInt32((int)score.layers.size());
		for (const auto& layer : score.layers)
		{
			writer.writeString(layer.name);
			writer.writeInt32(layer.hidden ? 1 : 0);
		}

		uint32_t waypointsAddress = writer.getStreamPosition();
		writer.writeInt32((int)score.waypoints.size());
		for (const auto& wp : score.waypoints)
		{
			writer.writeString(wp.name);
			writer.writeInt32(wp.tick);
		}

		uint32_t tapsAddress = writer.getStreamPosition();
		int noteCount = 0;
		writer.writeNull(sizeof(uint32_t));
		for (const auto& [_, note] : score.notes)
		{
			if (note.isHold())
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
			writer.writeInt32(static_cast<int>(hold.startType));
			writer.writeInt32(static_cast<int>(hold.endType));
			writer.writeInt32(static_cast<int>(hold.fadeType));
			writer.writeInt32(static_cast<int>(hold.guideColor));
			writer.writeInt32(hold.dummy ? 1 : 0);

			const Note& startNote = score.notes.at(hold.start.ID);
			writeNote(startNote, writer);
			writer.writeInt32(static_cast<int>(hold.start.ease));

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

		writer.seek(offsetsAddress);
		writer.writeInt32(metadataAddress);
		writer.writeInt32(eventsAddress);
		writer.writeInt32(tapsAddress);
		writer.writeInt32(holdsAddress);

		writer.flush();
		writer.close();
	}

	Score NativeScoreSerializer::deserialize(std::string filename)
	{
		Score score;
		IO::BinaryReader reader(filename);
		if (!reader.isStreamValid())
			return score;

		std::string signature = reader.readString();
		if (signature != UC_MMWS_SIGNATURE)
			throw std::runtime_error("Invalid MMWS file. Unrecognized signature");

		int version = reader.readUInt32();
		(void)version;

		uint32_t metadataAddress = reader.readUInt32();
		uint32_t eventsAddress = reader.readUInt32();
		uint32_t tapsAddress = reader.readUInt32();
		uint32_t holdsAddress = reader.readUInt32();

		reader.seek(metadataAddress);
		score.metadata = readMetadata(reader);

		reader.seek(eventsAddress);
		readScoreEvents(score, reader);

		reader.seek(tapsAddress);
		int noteCount = reader.readUInt32();
		id_t nextID = 0;
		for (int i = 0; i < noteCount; ++i)
		{
			Note note = readNote(reader);
			note.ID = nextID;
			score.notes[nextID++] = note;
		}

		reader.seek(holdsAddress);
		int holdCount = reader.readUInt32();
		id_t nextHoldID = 0;
		for (int i = 0; i < holdCount; ++i)
		{
			HoldNote hold;
			hold.startType = static_cast<HoldNoteType>(reader.readUInt32());
			hold.endType = static_cast<HoldNoteType>(reader.readUInt32());
			hold.fadeType = static_cast<FadeType>(reader.readUInt32());
			hold.guideColor = static_cast<GuideColor>(reader.readUInt32());
			hold.dummy = reader.readUInt32() != 0;

			Note startNote = readNote(reader);
			startNote.ID = nextID++;
			hold.start.ID = startNote.ID;
			hold.start.ease = static_cast<EaseType>(reader.readUInt32());
			hold.start.type = HoldStepType::Normal;
			score.notes[startNote.ID] = startNote;

			int stepCount = reader.readUInt32();
			for (int j = 0; j < stepCount; ++j)
			{
				Note mid = readNote(reader);
				mid.ID = nextID++;
				HoldStep step;
				step.ID = mid.ID;
				step.type = static_cast<HoldStepType>(reader.readUInt32());
				step.ease = static_cast<EaseType>(reader.readUInt32());
				hold.steps.push_back(step);
				score.notes[mid.ID] = mid;
			}

			Note endNote = readNote(reader);
			endNote.ID = nextID++;
			hold.end = endNote.ID;
			score.notes[endNote.ID] = endNote;

			score.holdNotes[nextHoldID++] = hold;
		}

		reader.close();
		return score;
	}

	bool NativeScoreSerializer::canSerialize(const Score&)
	{
		return true;
	}
}
