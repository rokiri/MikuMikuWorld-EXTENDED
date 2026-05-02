#pragma once

#include "ScoreSerializer.h"
#include "BinaryReader.h"
#include "BinaryWriter.h"

namespace MikuMikuWorld
{
	enum LegacyNoteType;
	class NativeScoreSerializer : public ScoreSerializer
	{
	  private:
		struct ScoreVersion;

		Note readNote(LegacyNoteType type, IO::BinaryReader& reader, const ScoreVersion& version);
		ScoreMetadata readMetadata(IO::BinaryReader& reader, const ScoreVersion& version);
		void readScoreEvents(Score& score, IO::BinaryReader& reader, const ScoreVersion& version);
		void writeNote(const Note& note, IO::BinaryWriter& writer);
		void writeMetadata(const ScoreMetadata& metadata, IO::BinaryWriter& writer);
		void writeScoreEvents(const Score& score, IO::BinaryWriter& writer);

	  public:
		void serialize(const SerializingScore& data, std::string filename) override;
		SerializingScore deserialize(std::string filename) override;

		static Result canSerialize(const SerializingScore& data);
	};

	class LegacyNativeScoreSerializer : public NativeScoreSerializer
	{
		void writeNote(LegacyNoteType type, const Note& note, IO::BinaryWriter& writer);
		void writeMetadata(const ScoreMetadata& metadata, IO::BinaryWriter& writer);
		void writeScoreEvents(const Score& score, IO::BinaryWriter& writer);

	  public:
		void serialize(const SerializingScore& data, std::string filename) override;

		static Result canSerialize(const SerializingScore& data);
	};
}