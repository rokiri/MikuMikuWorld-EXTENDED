#include "UscSerializer.h"
#include "IO.h"
#include "File.h"
#include "JsonIO.h"

using json = nlohmann::json;

namespace MikuMikuWorld
{
	void UscSerializer::serialize(const Score& score, std::string filename)
	{
		json usc = scoreToUsc(score);
		IO::File uscfile(filename, IO::FileMode::Write);
		uscfile.write(usc.dump(minify ? -1 : 4));
		uscfile.flush();
		uscfile.close();
	}

	Score UscSerializer::deserialize(std::string filename)
	{
		IO::File uscFile(filename, IO::FileMode::Read);
		json usc = json::parse(uscFile.readAllText());
		uscFile.close();
		return uscToScore(usc);
	}

	json UscSerializer::scoreToUsc(const Score& score)
	{
		json vusc;
		json usc;

		usc["offset"] = score.metadata.musicOffset / -1000.0f;

		std::vector<json> objects;

		for (const auto& bpm : score.tempoChanges)
		{
			json obj;
			obj["type"] = "bpm";
			obj["beat"] = bpm.tick / (double)TICKS_PER_BEAT;
			obj["bpm"] = bpm.bpm;
			objects.push_back(obj);
		}

		for (int i = 0; i < score.layers.size(); ++i)
		{
			json timeScaleGroup;
			timeScaleGroup["type"] = "timeScaleGroup";
			std::vector<json> timeScaleObjects;
			for (const auto& [_, hs] : score.hiSpeedChanges)
			{
				if (hs.layer != i)
				{
					continue;
				}
				json obj;
				obj["beat"] = hs.tick / (double)TICKS_PER_BEAT;
				obj["timeScale"] = hs.speed;
				timeScaleObjects.push_back(obj);
			}
			timeScaleGroup["changes"] = timeScaleObjects;
			objects.push_back(timeScaleGroup);
		}

		for (const auto& [_, note] : score.notes)
		{
			if (note.getType() == NoteType::Tap)
			{
				json obj;
				obj["type"] = "single";
				obj["beat"] = note.tick / (double)TICKS_PER_BEAT;
				obj["size"] = note.width / 2.0;
				obj["lane"] = note.lane - 6 + (note.width / 2.0);
				obj["critical"] = note.critical;
				obj["trace"] = note.friction;
				if (note.flick != FlickType::None)
				{
					obj["direction"] = note.flick == FlickType::Default ? "up"
					                   : note.flick == FlickType::Left  ? "left"
					                                                    : "right";
				}
				obj["timeScaleGroup"] = note.layer;
				if (note.dummy)
					obj["dummy"] = true;
				objects.push_back(obj);
			}
			else if (note.getType() == NoteType::Damage)
			{
				json obj;
				obj["type"] = "damage";
				obj["beat"] = note.tick / (double)TICKS_PER_BEAT;
				obj["size"] = note.width / 2.0;
				obj["lane"] = note.lane - 6 + (note.width / 2.0);
				obj["timeScaleGroup"] = note.layer;
				if (note.dummy)
					obj["dummy"] = true;
				objects.push_back(obj);
			}
		}
		for (const auto& [_, note] : score.holdNotes)
		{
			json obj;
			if (note.isGuide())
			{
				obj["type"] = "guide";
				auto& start = score.notes.at(note.start.ID);
				obj["color"] = guideColors[(int)note.guideColor];
				obj["fade"] = note.fadeType == FadeType::None ? "none"
				              : note.fadeType == FadeType::In ? "in"
				                                              : "out";

				std::vector<json> steps;
				steps.reserve(note.steps.size() + 1);

				json startStep;
				startStep["beat"] = start.tick / (double)TICKS_PER_BEAT;
				startStep["size"] = start.width / 2.0;
				startStep["lane"] = start.lane - 6 + (start.width / 2.0);
				startStep["ease"] = easeNames[(int)note.start.ease];
				startStep["timeScaleGroup"] = start.layer;
				if (start.dummy)
					startStep["dummy"] = true;
				steps.push_back(startStep);

				for (const auto& step : note.steps)
				{
					json stepObj;
					auto& stepNote = score.notes.at(step.ID);
					stepObj["beat"] = stepNote.tick / (double)TICKS_PER_BEAT;
					stepObj["size"] = stepNote.width / 2.0;
					stepObj["lane"] = stepNote.lane - 6 + (stepNote.width / 2.0);
					stepObj["ease"] = easeNames[(int)step.ease];
					stepObj["timeScaleGroup"] = stepNote.layer;
					steps.push_back(stepObj);
				}

				json endStep;
				auto& end = score.notes.at(note.end);
				endStep["beat"] = end.tick / (double)TICKS_PER_BEAT;
				endStep["size"] = end.width / 2.0;
				endStep["lane"] = end.lane - 6 + (end.width / 2.0);
				endStep["ease"] = "linear";
				endStep["timeScaleGroup"] = end.layer;
				steps.push_back(endStep);

				obj["midpoints"] = steps;
				objects.push_back(obj);
				continue;
			}
			obj["type"] = "slide";
			auto& start = score.notes.at(note.start.ID);
			obj["critical"] = start.critical;

			std::vector<json> steps;
			steps.reserve(note.steps.size() + 1);
			json startStep;
			startStep["type"] = "start";
			startStep["beat"] = start.tick / (double)TICKS_PER_BEAT;
			startStep["size"] = start.width / 2.0;
			startStep["lane"] = start.lane - 6 + (start.width / 2.0);
			startStep["critical"] = start.critical;
			startStep["ease"] = easeNames[(int)note.start.ease];
			startStep["judgeType"] = note.startType == HoldNoteType::Hidden ? "none"
			                         : start.friction                       ? "trace"
			                                                                : "normal";
			startStep["timeScaleGroup"] = start.layer;
			steps.push_back(startStep);

			for (const auto& step : note.steps)
			{
				json stepObj;
				auto& stepNote = score.notes.at(step.ID);
				stepObj["type"] = step.type == HoldStepType::Skip ? "attach" : "tick";
				stepObj["beat"] = stepNote.tick / (double)TICKS_PER_BEAT;
				stepObj["size"] = stepNote.width / 2.0;
				stepObj["lane"] = stepNote.lane - 6 + (stepNote.width / 2.0);
				if (step.type != HoldStepType::Hidden)
				{
					stepObj["critical"] = stepNote.critical;
				}
				stepObj["ease"] = easeNames[(int)step.ease];
				stepObj["timeScaleGroup"] = stepNote.layer;
				steps.push_back(stepObj);
			}

			json endStep;
			auto& end = score.notes.at(note.end);
			endStep["type"] = "end";
			endStep["beat"] = end.tick / (double)TICKS_PER_BEAT;
			endStep["size"] = end.width / 2.0;
			endStep["lane"] = end.lane - 6 + (end.width / 2.0);
			endStep["critical"] = end.critical;
			endStep["judgeType"] = note.endType == HoldNoteType::Hidden ? "none"
			                       : end.friction                       ? "trace"
			                                                            : "normal";
			if (end.flick != FlickType::None)
			{
				endStep["direction"] = end.flick == FlickType::Default ? "up"
				                       : end.flick == FlickType::Left  ? "left"
				                                                       : "right";
			}
			endStep["timeScaleGroup"] = end.layer;
			steps.push_back(endStep);

			obj["connections"] = steps;
			objects.push_back(obj);
		}

		usc["objects"] = objects;

		vusc["version"] = 2;
		vusc["usc"] = usc;
		return vusc;
	}

	Score UscSerializer::uscToScore(const json& vusc)
	{
		Score score;
		if (vusc["version"] != 2)
		{
			throw std::runtime_error("Invalid version");
		}
		json usc = vusc["usc"];
		score.layers.clear();
		score.hiSpeedChanges.clear();
		score.tempoChanges.clear();

		score.metadata.musicOffset = usc["offset"].get<float>() * -1000.0f;

		for (const auto& obj : usc["objects"].get<std::vector<json>>())
		{
			if (obj["type"] == "bpm")
			{
				score.tempoChanges.push_back(Tempo{
				    (int)(obj["beat"].get<double>() * TICKS_PER_BEAT), obj["bpm"].get<float>() });
			}
			else if (obj["type"] == "timeScaleGroup")
			{
				int index = score.layers.size();
				score.layers.push_back(Layer{ IO::formatString("#%d", index) });
				for (const auto& change : obj["changes"])
				{
					id_t id = Note::getNextID();
					score.hiSpeedChanges[id] =
					    HiSpeedChange{ id, (int)(change["beat"].get<double>() * TICKS_PER_BEAT),
						               change["timeScale"].get<float>(), index };
				}
			}
			else if (obj["type"] == "single")
			{
				Note note(NoteType::Tap);
				note.tick = obj["beat"].get<double>() * TICKS_PER_BEAT;
				note.width = obj["size"].get<float>() * 2;
				note.lane = obj["lane"].get<float>() + 6 - obj["size"].get<float>();
				note.critical = obj["critical"].get<bool>();
				note.friction = obj["trace"].get<bool>();
				if (obj.contains("direction"))
				{
					std::string dir = obj["direction"].get<std::string>();
					note.flick = dir == "up"     ? FlickType::Default
					             : dir == "left" ? FlickType::Left
					                             : FlickType::Right;
				}
				else
				{
					note.flick = FlickType::None;
				}
				note.layer = obj["timeScaleGroup"].get<int>();
				if (obj.contains("dummy"))
					note.dummy = obj["dummy"].get<bool>();
				else
					note.dummy = false;
				note.ID = Note::getNextID();
				score.notes[note.ID] = note;
			}
			else if (obj["type"] == "damage")
			{
				Note note(NoteType::Damage);
				note.tick = obj["beat"].get<double>() * TICKS_PER_BEAT;
				note.width = obj["size"].get<float>() * 2;
				note.lane = obj["lane"].get<float>() + 6 - obj["size"].get<float>();
				note.layer = obj["timeScaleGroup"].get<int>();
				note.ID = Note::getNextID();
				score.notes[note.ID] = note;
			}
			else if (obj["type"] == "guide")
			{
				HoldNote hold;

				auto color = obj["color"].get<std::string>();

				if (color == "neutral")
				{
					hold.guideColor = GuideColor::Neutral;
				}
				else if (color == "red")
				{
					hold.guideColor = GuideColor::Red;
				}
				else if (color == "green")
				{
					hold.guideColor = GuideColor::Green;
				}
				else if (color == "blue")
				{
					hold.guideColor = GuideColor::Blue;
				}
				else if (color == "yellow")
				{
					hold.guideColor = GuideColor::Yellow;
				}
				else if (color == "purple")
				{
					hold.guideColor = GuideColor::Purple;
				}
				else if (color == "cyan")
				{
					hold.guideColor = GuideColor::Cyan;
				}
				else if (color == "black")
				{
					hold.guideColor = GuideColor::Black;
				}
				hold.fadeType = obj["fade"].get<std::string>() == "none" ? FadeType::None
				                : obj["fade"].get<std::string>() == "in" ? FadeType::In
				                                                         : FadeType::Out;

				for (int i = 0; i < obj["midpoints"].size(); i++)
				{
					const auto& step = obj["midpoints"][i];
					if (i == 0)
					{
						Note startNote(NoteType::Hold);
						startNote.tick = step["beat"].get<double>() * TICKS_PER_BEAT;
						startNote.lane = step["lane"].get<float>() + 6 - step["size"].get<float>();
						startNote.layer = step["timeScaleGroup"].get<int>();
						startNote.ID = Note::getNextID();
						startNote.width = step["size"].get<float>() * 2;
						score.notes[startNote.ID] = startNote;
						hold.start.ID = startNote.ID;

						std::string ease = jsonIO::tryGetValue(step, "ease", std::string("linear"));
						if (ease == "in")
						{
							hold.start.ease = EaseType::EaseIn;
						}
						else if (ease == "out")
						{
							hold.start.ease = EaseType::EaseOut;
						}
						else if (ease == "inout")
						{
							hold.start.ease = EaseType::EaseInOut;
						}
						else if (ease == "outin")
						{
							hold.start.ease = EaseType::EaseOutIn;
						}
						else
						{
							hold.start.ease = EaseType::Linear;
						}
						hold.startType = HoldNoteType::Guide;
					}
					else if (i == obj["midpoints"].size() - 1)
					{
						Note endNote(NoteType::HoldEnd);
						endNote.tick = step["beat"].get<double>() * TICKS_PER_BEAT;
						endNote.lane = step["lane"].get<float>() + 6 - step["size"].get<float>();
						endNote.layer = step["timeScaleGroup"].get<int>();
						endNote.ID = Note::getNextID();
						endNote.parentID = hold.start.ID;
						endNote.width = step["size"].get<float>() * 2;
						score.notes[endNote.ID] = endNote;
						hold.end = endNote.ID;
						hold.endType = HoldNoteType::Guide;
					}
					else
					{
						HoldStep s;
						Note mid(NoteType::HoldMid);
						mid.tick = step["beat"].get<double>() * TICKS_PER_BEAT;
						mid.lane = step["lane"].get<float>() + 6 - step["size"].get<float>();
						mid.layer = step["timeScaleGroup"].get<int>();
						mid.ID = Note::getNextID();
						mid.parentID = hold.start.ID;
						mid.width = step["size"].get<float>() * 2;
						score.notes[mid.ID] = mid;
						s.ID = mid.ID;

						std::string ease = jsonIO::tryGetValue(step, "ease", std::string("linear"));
						if (ease == "in")
						{
							s.ease = EaseType::EaseIn;
						}
						else if (ease == "out")
						{
							s.ease = EaseType::EaseOut;
						}
						else if (ease == "inout")
						{
							s.ease = EaseType::EaseInOut;
						}
						else if (ease == "outin")
						{
							s.ease = EaseType::EaseOutIn;
						}
						else
						{
							s.ease = EaseType::Linear;
						}
						s.type = HoldStepType::Hidden;
						hold.steps.push_back(s);
					}
				}
				score.holdNotes[hold.start.ID] = hold;
			}
			else if (obj["type"] == "slide")
			{
				HoldNote hold;
				hold.fadeType = FadeType::None;

				auto connections = obj["connections"].get<std::vector<json>>();
				std::stable_sort(connections.begin(), connections.end(),
				                 [](const json& a, const json& b)
				                 {
					                 if (a["type"] == "start")
					                 {
						                 return true;
					                 }
					                 else if (b["type"] == "start")
					                 {
						                 return false;
					                 }
					                 else if (a["type"] == "end")
					                 {
						                 return false;
					                 }
					                 else if (b["type"] == "end")
					                 {
						                 return true;
					                 }
					                 return a["beat"].get<double>() < b["beat"].get<float>();
				                 });

				bool isCritical = false;
				for (const auto& step : connections)
				{
					auto type = step["type"].get<std::string>();
					if (type == "start")
					{
						Note startNote(NoteType::Hold);
						startNote.tick = step["beat"].get<double>() * TICKS_PER_BEAT;
						startNote.lane = step["lane"].get<float>() + 6 - step["size"].get<float>();
						startNote.layer = step["timeScaleGroup"].get<int>();
						startNote.critical = step["critical"].get<bool>();
						isCritical = startNote.critical;
						startNote.width = step["size"].get<float>() * 2;
						if (step["judgeType"].get<std::string>() == "trace")
						{
							startNote.friction = true;
							hold.startType = HoldNoteType::Normal;
						}
						else if (step["judgeType"].get<std::string>() == "none")
						{
							hold.startType = HoldNoteType::Hidden;
						}
						else
						{
							hold.startType = HoldNoteType::Normal;
						}
						startNote.ID = Note::getNextID();
						score.notes[startNote.ID] = startNote;
						hold.start.ID = startNote.ID;

						std::string ease = jsonIO::tryGetValue(step, "ease", std::string("linear"));
						if (ease == "in")
						{
							hold.start.ease = EaseType::EaseIn;
						}
						else if (ease == "out")
						{
							hold.start.ease = EaseType::EaseOut;
						}
						else if (ease == "inout")
						{
							hold.start.ease = EaseType::EaseInOut;
						}
						else if (ease == "outin")
						{
							hold.start.ease = EaseType::EaseOutIn;
						}
						else
						{
							hold.start.ease = EaseType::Linear;
						}
					}
					else if (type == "end")
					{
						Note endNote(NoteType::HoldEnd);
						endNote.tick = step["beat"].get<double>() * TICKS_PER_BEAT;
						endNote.lane = step["lane"].get<float>() + 6 - step["size"].get<float>();
						endNote.layer = step["timeScaleGroup"].get<int>();
						endNote.width = step["size"].get<float>() * 2;
						endNote.critical = isCritical || step["critical"].get<bool>();
						endNote.flick = step.contains("direction")
						                    ? step["direction"].get<std::string>() == "up"
						                          ? FlickType::Default
						                      : step["direction"].get<std::string>() == "left"
						                          ? FlickType::Left
						                          : FlickType::Right
						                    : FlickType::None;
						endNote.ID = Note::getNextID();
						endNote.parentID = hold.start.ID;

						if (step["judgeType"].get<std::string>() == "trace")
						{
							endNote.friction = true;
							hold.endType = HoldNoteType::Normal;
						}
						else if (step["judgeType"].get<std::string>() == "none")
						{
							hold.endType = HoldNoteType::Hidden;
						}
						else
						{
							hold.endType = HoldNoteType::Normal;
						}
						score.notes[endNote.ID] = endNote;
						hold.end = endNote.ID;
					}
					else
					{
						HoldStep s;
						Note mid(NoteType::HoldMid);
						mid.tick = step["beat"].get<double>() * TICKS_PER_BEAT;
						mid.lane = step["lane"].get<float>() + 6 - step["size"].get<float>();
						mid.width = step["size"].get<float>() * 2;
						mid.layer = step["timeScaleGroup"].get<int>();
						mid.critical = isCritical;
						mid.ID = Note::getNextID();
						mid.parentID = hold.start.ID;
						score.notes[mid.ID] = mid;
						s.ID = mid.ID;

						std::string ease = jsonIO::tryGetValue(step, "ease", std::string("linear"));
						if (ease == "in")
						{
							s.ease = EaseType::EaseIn;
						}
						else if (ease == "out")
						{
							s.ease = EaseType::EaseOut;
						}
						else if (ease == "inout")
						{
							s.ease = EaseType::EaseInOut;
						}
						else if (ease == "outin")
						{
							s.ease = EaseType::EaseOutIn;
						}
						else
						{
							s.ease = EaseType::Linear;
						}

						if (type == "tick")
						{
							if (step.contains("critical"))
							{
								s.type = HoldStepType::Normal;
							}
							else
							{
								s.type = HoldStepType::Hidden;
							}
						}
						else if (type == "attach")
						{
							s.type = HoldStepType::Skip;
						}
						hold.steps.push_back(s);
					}
				}

				score.holdNotes[hold.start.ID] = hold;
			}
		}

		if (score.layers.size() == 0)
		{
			score.layers.push_back(Layer{ "#0" });
		}
		if (score.tempoChanges.size() == 0)
		{
			score.tempoChanges.push_back(Tempo{ 0, 120 });
		}

		return score;
	}

	bool UscSerializer::canSerialize(const Score& score) { return true; }
}