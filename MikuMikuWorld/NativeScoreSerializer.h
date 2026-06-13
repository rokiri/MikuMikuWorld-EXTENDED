#pragma once

#include "ScoreSerializer.h"
#include "BinaryReader.h"
#include "BinaryWriter.h"

namespace MikuMikuWorld
{
	class NativeScoreSerializer : public ScoreSerializer
	{
	  public:
		void serialize(const Score& score, std::string filename) override;
		Score deserialize(std::string filename) override;

		static bool canSerialize(const Score& score);
	};
}
