#pragma once
#include "ScoreSerializer.h"

namespace MikuMikuWorld
{
	struct SUS;

	class SusSerializer : public ScoreSerializer
	{
		std::string exportComment;

	  public:
		SusSerializer();

		static Score susToScore(const SUS& sus);
		static SUS scoreToSus(const Score& score);

		void serialize(const Score& score, std::string filename) override;
		Score deserialize(std::string filename) override;

		static bool canSerialize(const Score& score);
	};
}
