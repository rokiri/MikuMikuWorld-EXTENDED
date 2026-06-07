#pragma once
#include "Constants.h"
#include "Note.h"
#include "Tempo.h"
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace MikuMikuWorld
{
	id_t getNextSkillID();
	id_t getNextHiSpeedID();

	struct SkillTrigger
	{
		id_t ID;
		int tick;
	};

	struct Fever
	{
		int startTick;
		int endTick;
	};

	struct Layer
	{
		std::string name;
		bool hidden = false;

		// --- フォルダ機能用の拡張 ---
		bool isFolder = false;    // 自分が「フォルダ」であるかどうか
		bool inFolder = false;    // 自分が直上のフォルダに「属している」かどうか
		bool isCollapsed = false; // フォルダが折りたたまれているかどうか（UI用）
	};

	struct Waypoint
	{
		std::string name;
		int tick;
	};
	
	struct HiSpeedChange
	{
		id_t ID;
		int tick;
		float speed;
		int layer = 0;
		float skips = 0;
		HiSpeedEaseType ease;
		bool hideNotes;
	};

	struct ScoreMetadata
	{
		std::string title;
		std::string artist;
		std::string author;
		std::string musicFile;
		std::string jacketFile;
		float musicOffset;

		int laneExtension = 0;
	};

	struct Score
	{
		ScoreMetadata metadata;
		std::unordered_map<id_t, Note> notes;
		std::unordered_map<id_t, HoldNote> holdNotes;
		std::vector<Tempo> tempoChanges;
		std::map<int, TimeSignature> timeSignatures;
		std::unordered_map<id_t, HiSpeedChange> hiSpeedChanges;
		std::unordered_map<id_t, SkillTrigger> skills;
		Fever fever;

		std::vector<Layer> layers{ { Layer{ "default" } } };
		std::vector<Waypoint> waypoints;

		Score();
	};
}
