#pragma once
#include <json.hpp>
#include <unordered_set>

namespace mmw = MikuMikuWorld;

namespace jsonIO
{
	static bool keyExists(const nlohmann::json& js, const char* key)
	{
		return (js.find(key) != js.end());
	}

	static bool keyExists(const nlohmann::json& js, const char* key, nlohmann::json::value_t type)
	{
		auto it = js.find(key);
		return (it != js.end()) && it->type() == type;
	}

	static bool arrayHasData(const nlohmann::json& js, const char* key)
	{
		return jsonIO::keyExists(js, key) && js[key].is_array() && js[key].size();
	}

	template <typename T> T tryGetValue(const nlohmann::json& js, const char* key, T def = {})
	{
		return keyExists(js, key) ? js[key].template get<T>() : std::move(def);
	}

	template <typename T, typename TKey>
	static auto optional_get_to(const nlohmann::json& j, TKey&& k, T& v)
	    -> decltype(j.get_to(v), T())
	{
		if (j.contains(k))
			return j[k].get_to(v);
		return v;
	}

	mmw::Note jsonToNote(const nlohmann::json& data, mmw::NoteType type);

	nlohmann::json noteToJson(const mmw::Note& note);

	nlohmann::json noteSelectionToJson(const mmw::Score& score,
	                                   const std::unordered_set<mmw::id_t>& selection,
	                                   const std::unordered_set<mmw::id_t>& hiSpeedSelection,
	                                   int baseTick);
}

namespace std::chrono
{
	void to_json(nlohmann::json& j, const system_clock::time_point& tp);
	void from_json(const nlohmann::json& j, system_clock::time_point& tp);
}

namespace MikuMikuWorld
{
	struct Vector2;
	void to_json(nlohmann::json& j, const Vector2& vec);
	void from_json(const nlohmann::json& j, Vector2& vec);

	struct Color;
	void to_json(nlohmann::json& j, const Color& color);
	void from_json(const nlohmann::json& j, Color& color);

	struct InputConfiguration;
	void to_json(nlohmann::json& j, const InputConfiguration& input);
	void from_json(const nlohmann::json& j, InputConfiguration& input);

	struct ApplicationConfiguration;
	void to_json(nlohmann::json& j, const ApplicationConfiguration& cfg);
	void from_json(const nlohmann::json& j, ApplicationConfiguration& cfg);
}