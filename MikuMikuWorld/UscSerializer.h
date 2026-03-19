#pragma once
#include "ScoreSerializer.h"
#include "json.hpp"

namespace MikuMikuWorld
{
	class UscSerializer : public ScoreSerializer
	{
	  public:
		static nlohmann::json scoreToUsc(const SerializingScore& data);
		static SerializingScore uscToScore(const nlohmann::json& usc);

		UscSerializer(bool minifyUsc = true) : minify(minifyUsc) {}

		void serialize(const SerializingScore& data, std::string filename) override;
		SerializingScore deserialize(std::string filename) override;

		static Result canSerialize(const SerializingScore& data);

		bool minify;
	};
}
