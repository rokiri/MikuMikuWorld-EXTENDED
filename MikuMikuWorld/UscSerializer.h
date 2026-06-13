#pragma once
#include "ScoreSerializer.h"
#include "json.hpp"

namespace MikuMikuWorld
{
	class UscSerializer : public ScoreSerializer
	{
	  public:
		static nlohmann::json scoreToUsc(const Score& score);
		static Score uscToScore(const nlohmann::json& usc);

		UscSerializer(bool minifyUsc = true) : minify(minifyUsc) {}

		void serialize(const Score& score, std::string filename) override;
		Score deserialize(std::string filename) override;

		static bool canSerialize(const Score& score);

		bool minify;
	};
}
