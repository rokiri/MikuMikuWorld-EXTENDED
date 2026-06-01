#include "UscSerializer.h"
#include "IO.h"
#include "File.h"
#include "JsonIO.h"

using json = nlohmann::json;

namespace MikuMikuWorld
{
	// -------------------------------------------------------------------------
	// Helpers
	// -------------------------------------------------------------------------

	static EaseType parseEase(const std::string& ease)
	{
		if (ease == "in")
			return EaseType::EaseIn;
		if (ease == "out")
			return EaseType::EaseOut;
		if (ease == "inout")
			return EaseType::EaseInOut;
		if (ease == "outin")
			return EaseType::EaseOutIn;
		return EaseType::Linear;
	}

	// A hold is a guide when its first separator has the Guide flag
	static bool holdIsGuide(const HoldNote& hold)
	{
		return !hold.separators.empty() && hold.separators.front().isGuide();
	}

	// -------------------------------------------------------------------------
	// Public interface
	// -------------------------------------------------------------------------

	void UscSerializer::serialize(const SerializingScore& data, std::string filename)
	{
		json usc = scoreToUsc(data);
		IO::File uscfile(filename, IO::FileMode::Write);
		uscfile.write(usc.dump(minify ? -1 : 4));
		uscfile.flush();
		uscfile.close();
	}

	SerializingScore UscSerializer::deserialize(std::string filename)
	{
		IO::File uscFile(filename, IO::FileMode::Read);
		json usc = json::parse(uscFile.readAllText());
		uscFile.close();
		return uscToScore(usc);
	}

	// -------------------------------------------------------------------------
	// scoreToUsc
	// -------------------------------------------------------------------------

	json UscSerializer::scoreToUsc(const SerializingScore& data)
	{
		const Score& score = data.score;
		const ScoreMetadata& metadata = data.metadata;

		json vusc;
		json usc;
		usc["offset"] = metadata.musicOffset / -1000.0f;

		std::vector<json> objects;

		// --- BPM changes ---
		for (const auto& [tick, tempo] : score.tempoChanges)
		{
			json obj;
			obj["type"] = "bpm";
			obj["beat"] = tick / (double)TICKS_PER_QUARTER;
			obj["bpm"] = tempo.quarterPerMinute;
			objects.push_back(obj);
		}

		// --- TimeScaleGroups (one per layer) ---
		for (int i = 0; i < (int)score.layers.size(); ++i)
		{
			json timeScaleGroup;
			timeScaleGroup["type"] = "timeScaleGroup";

			std::vector<json> changes;
			for (const auto& [tick, hs] : score.layers[i].hiSpeedChanges)
			{
				json ch;
				ch["beat"] = tick / (double)TICKS_PER_QUARTER;
				ch["timeScale"] = hs.speed;
				changes.push_back(ch);
			}

			timeScaleGroup["changes"] = changes;
			objects.push_back(timeScaleGroup);
		}

		// --- Standalone notes (not part of a hold) ---
		for (const auto& [_, note] : score.notes)
		{
			if (note.isHold())
				continue;

			if (note.type == NoteType::Tap)
			{
				json obj;
				obj["type"] = "single";
				obj["beat"] = note.tick / (double)TICKS_PER_QUARTER;
				obj["size"] = note.width / 2.0f;
				obj["lane"] = note.lane - 6.0f + (note.width / 2.0f);
				obj["critical"] = note.isCrit();
				obj["trace"] = note.isTrace();
				obj["timeScaleGroup"] = (int)note.layer;

				if (note.flick != FlickType::None)
				{
					obj["direction"] = note.flick == FlickType::Default ? "up"
					                   : note.flick == FlickType::Left  ? "left"
					                                                    : "right";
				}
				if (note.isDummy())
					obj["dummy"] = true;

				objects.push_back(obj);
			}
			else if (note.type == NoteType::Damage)
			{
				json obj;
				obj["type"] = "damage";
				obj["beat"] = note.tick / (double)TICKS_PER_QUARTER;
				obj["size"] = note.width / 2.0f;
				obj["lane"] = note.lane - 6.0f + (note.width / 2.0f);
				obj["timeScaleGroup"] = (int)note.layer;

				if (note.isDummy())
					obj["dummy"] = true;

				objects.push_back(obj);
			}
		}

		// --- Hold notes ---
		for (const auto& [_, hold] : score.holdNotes)
		{
			if (hold.steps.size() < 2)
				continue;

			const Note& startNote = score.notes.at(hold.steps.front());
			const Note& endNote = score.notes.at(hold.steps.back());

			if (holdIsGuide(hold))
			{
				// ---- guide ----
				json obj;
				obj["type"] = "guide";

				const HoldNoteStep& firstSep = hold.separators.front();
				obj["color"] = guideColors[(int)firstSep.guideColor];
				obj["fade"] = hold.fadeType == FadeType::None ? "none"
				              : hold.fadeType == FadeType::In ? "in"
				                                              : "out";

				std::vector<json> midpoints;
				for (int si = 0; si < (int)hold.steps.size(); ++si)
				{
					const Note& n = score.notes.at(hold.steps[si]);
					json stepObj;
					stepObj["beat"] = n.tick / (double)TICKS_PER_QUARTER;
					stepObj["size"] = n.width / 2.0f;
					stepObj["lane"] = n.lane - 6.0f + (n.width / 2.0f);
					stepObj["timeScaleGroup"] = (int)n.layer;
					stepObj["ease"] =
					    (si == (int)hold.steps.size() - 1) ? "linear" : easeTypes[(int)n.ease];
					if (si == 0 && n.isDummy())
						stepObj["dummy"] = true;

					midpoints.push_back(stepObj);
				}
				obj["midpoints"] = midpoints;
				objects.push_back(obj);
			}
			else
			{
				// ---- slide ----
				json obj;
				obj["type"] = "slide";
				obj["critical"] = startNote.isCrit();

				std::vector<json> connections;
				for (int si = 0; si < (int)hold.steps.size(); ++si)
				{
					const Note& n = score.notes.at(hold.steps[si]);
					json stepObj;
					stepObj["beat"] = n.tick / (double)TICKS_PER_QUARTER;
					stepObj["size"] = n.width / 2.0f;
					stepObj["lane"] = n.lane - 6.0f + (n.width / 2.0f);
					stepObj["timeScaleGroup"] = (int)n.layer;

					if (si == 0)
					{
						stepObj["type"] = "start";
						stepObj["critical"] = n.isCrit();
						stepObj["ease"] = easeTypes[(int)n.ease];
						stepObj["judgeType"] = n.isHidden()  ? "none"
						                       : n.isTrace() ? "trace"
						                                     : "normal";
					}
					else if (si == (int)hold.steps.size() - 1)
					{
						stepObj["type"] = "end";
						stepObj["critical"] = n.isCrit();
						stepObj["judgeType"] = n.isHidden()  ? "none"
						                       : n.isTrace() ? "trace"
						                                     : "normal";
						if (n.flick != FlickType::None)
						{
							stepObj["direction"] = n.flick == FlickType::Default ? "up"
							                       : n.flick == FlickType::Left  ? "left"
							                                                     : "right";
						}
					}
					else
					{
						stepObj["type"] = n.isAttached() ? "attach" : "tick";
						stepObj["ease"] = easeTypes[(int)n.ease];
						if (!n.isHidden())
							stepObj["critical"] = n.isCrit();
					}

					connections.push_back(stepObj);
				}
				obj["connections"] = connections;
				objects.push_back(obj);
			}
		}

		usc["objects"] = objects;
		vusc["version"] = 2;
		vusc["usc"] = usc;
		return vusc;
	}

	// -------------------------------------------------------------------------
	// uscToScore
	// -------------------------------------------------------------------------

	SerializingScore UscSerializer::uscToScore(const json& vusc)
	{
		Score score;
		ScoreMetadata metadata;

		if (vusc["version"] != 2)
			throw std::runtime_error("Invalid USC version");

		const json& usc = vusc["usc"];

		score.layers.clear();
		score.tempoChanges.clear();

		metadata.musicOffset = usc["offset"].get<float>() * -1000.0f;

		// Counters — same pattern as NativeScoreSerializer
		id_t nextNoteID = 0;
		id_t nextHoldID = 0;

		for (const auto& obj : usc["objects"].get<std::vector<json>>())
		{
			const std::string objType = obj["type"].get<std::string>();

			if (objType == "bpm")
			{
				tick_t tick = (tick_t)(obj["beat"].get<double>() * TICKS_PER_QUARTER);
				score.tempoChanges[tick] = Tempo{ tick, obj["bpm"].get<float>() };
			}
			else if (objType == "timeScaleGroup")
			{
				int layerIndex = (int)score.layers.size();
				Layer newLayer(IO::formatString("#%d", layerIndex));

				for (const auto& change : obj["changes"])
				{
					tick_t tick = (tick_t)(change["beat"].get<double>() * TICKS_PER_QUARTER);
					HiSpeed hs;
					hs.tick = tick;
					hs.layer = layerIndex;
					hs.speed = change["timeScale"].get<float>();
					newLayer.hiSpeedChanges[tick] = hs;
				}

				score.layers.push_back(std::move(newLayer));
			}
			else if (objType == "single")
			{
				Note note;
				note.type = NoteType::Tap;
				note.tick = (tick_t)(obj["beat"].get<double>() * TICKS_PER_QUARTER);
				note.width = obj["size"].get<float>() * 2.0f;
				note.lane = obj["lane"].get<float>() + 6.0f - obj["size"].get<float>();
				note.layer = obj["timeScaleGroup"].get<id_t>();
				note.ID = nextNoteID++;

				if (obj["critical"].get<bool>())
					note.flag = setFlag(note.flag, NoteFlag::Critical);
				if (obj["trace"].get<bool>())
					note.flag = setFlag(note.flag, NoteFlag::Trace);
				if (obj.contains("dummy") && obj["dummy"].get<bool>())
					note.flag = setFlag(note.flag, NoteFlag::Dummy);

				if (obj.contains("direction"))
				{
					const std::string dir = obj["direction"].get<std::string>();
					note.flick = dir == "up"     ? FlickType::Default
					             : dir == "left" ? FlickType::Left
					                             : FlickType::Right;
				}

				score.notes[note.ID] = note;
			}
			else if (objType == "damage")
			{
				Note note;
				note.type = NoteType::Damage;
				note.tick = (tick_t)(obj["beat"].get<double>() * TICKS_PER_QUARTER);
				note.width = obj["size"].get<float>() * 2.0f;
				note.lane = obj["lane"].get<float>() + 6.0f - obj["size"].get<float>();
				note.layer = obj["timeScaleGroup"].get<id_t>();
				note.ID = nextNoteID++;

				if (obj.contains("dummy") && obj["dummy"].get<bool>())
					note.flag = setFlag(note.flag, NoteFlag::Dummy);

				score.notes[note.ID] = note;
			}
			else if (objType == "guide")
			{
				HoldNote hold;
				hold.ID = nextHoldID;

				const std::string color = obj["color"].get<std::string>();
				GuideColor gc = GuideColor::Neutral;
				if (color == "red")
					gc = GuideColor::Red;
				else if (color == "green")
					gc = GuideColor::Green;
				else if (color == "blue")
					gc = GuideColor::Blue;
				else if (color == "yellow")
					gc = GuideColor::Yellow;
				else if (color == "purple")
					gc = GuideColor::Purple;
				else if (color == "cyan")
					gc = GuideColor::Cyan;
				else if (color == "black")
					gc = GuideColor::Black;

				const std::string fadeStr = obj["fade"].get<std::string>();
				hold.fadeType = fadeStr == "none" ? FadeType::None
				                : fadeStr == "in" ? FadeType::In
				                                  : FadeType::Out;

				const auto& midpoints = obj["midpoints"];
				for (int i = 0; i < (int)midpoints.size(); ++i)
				{
					const auto& step = midpoints[i];

					Note n;
					n.type = NoteType::Tap;
					n.tick = (tick_t)(step["beat"].get<double>() * TICKS_PER_QUARTER);
					n.width = step["size"].get<float>() * 2.0f;
					n.lane = step["lane"].get<float>() + 6.0f - step["size"].get<float>();
					n.layer = step["timeScaleGroup"].get<id_t>();
					n.holdID = nextHoldID;
					n.ID = nextNoteID++;
					n.ease = parseEase(jsonIO::tryGetValue(step, "ease", std::string("linear")));

					if (step.contains("dummy") && step["dummy"].get<bool>())
						n.flag = setFlag(n.flag, NoteFlag::Dummy);

					score.notes[n.ID] = n;
					hold.steps.push_back(n.ID);
				}

				// First separator carries Guide flag + color (same as NativeSerializer)
				if (!hold.steps.empty())
				{
					HoldNoteStep sep;
					sep.ID = hold.steps.front();
					sep.flag = HoldNoteFlag::Guide;
					sep.guideColor = gc;
					hold.separators.push_back(sep);
				}

				hold.sortSteps(score.notes);
				score.holdNotes[nextHoldID++] = hold;
			}
			else if (objType == "slide")
			{
				HoldNote hold;
				hold.ID = nextHoldID;
				hold.fadeType = FadeType::None;

				auto connections = obj["connections"].get<std::vector<json>>();

				// Sort: start first, end last, mids by beat
				std::stable_sort(connections.begin(), connections.end(),
				                 [](const json& a, const json& b)
				                 {
					                 const std::string at = a["type"], bt = b["type"];
					                 if (at == "start")
						                 return true;
					                 if (bt == "start")
						                 return false;
					                 if (at == "end")
						                 return false;
					                 if (bt == "end")
						                 return true;
					                 return a["beat"].get<double>() < b["beat"].get<double>();
				                 });

				bool isCritical = false;

				for (const auto& step : connections)
				{
					const std::string st = step["type"].get<std::string>();

					Note n;
					n.type = NoteType::Tap;
					n.tick = (tick_t)(step["beat"].get<double>() * TICKS_PER_QUARTER);
					n.width = step["size"].get<float>() * 2.0f;
					n.lane = step["lane"].get<float>() + 6.0f - step["size"].get<float>();
					n.layer = step["timeScaleGroup"].get<id_t>();
					n.holdID = nextHoldID;
					n.ID = nextNoteID++;

					if (st == "start")
					{
						isCritical = step["critical"].get<bool>();
						if (isCritical)
							n.flag = setFlag(n.flag, NoteFlag::Critical);

						n.ease =
						    parseEase(jsonIO::tryGetValue(step, "ease", std::string("linear")));

						const std::string jt = step["judgeType"].get<std::string>();
						if (jt == "trace")
							n.flag = setFlag(n.flag, NoteFlag::Trace);
						if (jt == "none")
							n.flag = setFlag(n.flag, NoteFlag::Hidden);
					}
					else if (st == "end")
					{
						if (isCritical || step["critical"].get<bool>())
							n.flag = setFlag(n.flag, NoteFlag::Critical);

						const std::string jt = step["judgeType"].get<std::string>();
						if (jt == "trace")
							n.flag = setFlag(n.flag, NoteFlag::Trace);
						if (jt == "none")
							n.flag = setFlag(n.flag, NoteFlag::Hidden);

						if (step.contains("direction"))
						{
							const std::string dir = step["direction"].get<std::string>();
							n.flick = dir == "up"     ? FlickType::Default
							          : dir == "left" ? FlickType::Left
							                          : FlickType::Right;
						}
					}
					else // "tick" or "attach"
					{
						if (isCritical)
							n.flag = setFlag(n.flag, NoteFlag::Critical);

						n.ease =
						    parseEase(jsonIO::tryGetValue(step, "ease", std::string("linear")));

						if (st == "attach")
							n.flag = setFlag(n.flag, NoteFlag::Attached);
						else if (!step.contains("critical")) // tick with no critical = hidden
							n.flag = setFlag(n.flag, NoteFlag::Hidden);
					}

					score.notes[n.ID] = n;
					hold.steps.push_back(n.ID);
				}

				// First separator = normal (no Guide flag), inherits critical from start
				if (!hold.steps.empty())
				{
					HoldNoteStep sep;
					sep.ID = hold.steps.front();
					sep.flag = isCritical ? HoldNoteFlag::Critical : HoldNoteFlag::Normal;
					hold.separators.push_back(sep);
				}

				hold.sortSteps(score.notes);
				score.holdNotes[nextHoldID++] = hold;
			}
		}

		// Fallback defaults
		if (score.layers.empty())
			score.layers.emplace_back("#0");

		if (score.tempoChanges.empty())
			score.tempoChanges[0] = Tempo{ 0, 120.0f };

		return { score, metadata };
	}

	Result UscSerializer::canSerialize(const SerializingScore& data) { return Result::Ok(); }
}
