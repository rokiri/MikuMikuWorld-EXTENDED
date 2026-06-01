#include "SusSerializer.h"
#include "SUS.h"
#include "SusExporter.h"
#include "SusParser.h"
#include "Constants.h"

namespace MikuMikuWorld
{
	// SUS lane constants (standard PJSK/Sonolus 12-lane system)
	constexpr float MIN_LANE = 0.0f;
	constexpr float MAX_LANE = 11.0f;
	constexpr float MAX_NOTE_WIDTH = 12.0f;
}

namespace MikuMikuWorld
{
	// -------------------------------------------------------------------------
	// Helpers — defined before use
	// -------------------------------------------------------------------------

	static bool holdIsGuide(const HoldNote& hold)
	{
		return !hold.separators.empty() && hold.separators.front().isGuide();
	}

	static std::string noteKey(const SUSNote& note)
	{
		return IO::formatString("%d-%d", note.tick, note.lane);
	}

	static std::string noteKey(const Note& note)
	{
		return IO::formatString("%d-%g", note.tick, note.lane);
	}

	static std::pair<int, int> barLengthToFraction(float length, float fractionDenom)
	{
		int factor = 1;
		for (int i = 2; i < 10; ++i)
		{
			if (fmodf(factor * length, 1) == 0)
				return { (int)(factor * length), (int)std::pow(2, i) };
			factor *= 2;
		}
		return { 4, 4 };
	}

	// -------------------------------------------------------------------------
	// Public interface
	// -------------------------------------------------------------------------

	void SusSerializer::serialize(const SerializingScore& data, std::string filename)
	{
		SUS sus = scoreToSus(data);
		SusExporter exporter{};
		exporter.dump(sus, filename, exportComment);
	}

	SerializingScore SusSerializer::deserialize(std::string filename)
	{
		SUS sus = SusParser().parse(filename);
		return susToScore(sus);
	}

	Result SusSerializer::canSerialize(const SerializingScore& data)
	{
		const Score& score = data.score;

		if (data.metadata.laneExtension != 0)
			return Result(ResultStatus::Error, "Lane extension not supported in SUS");

		for (const auto& [_, note] : score.notes)
		{
			if (note.isHold())
				continue;
			if (floorf(note.width) != note.width || floorf(note.lane) != note.lane)
				return Result(ResultStatus::Error,
				              "Floating point lane/width not supported in SUS");
			if (note.layer != 0)
				return Result(ResultStatus::Error, "Multiple layers not supported in SUS");
			if (note.isDummy())
				return Result(ResultStatus::Error, "Dummy notes not supported in SUS");
			float maxWidth = MAX_NOTE_WIDTH - note.lane;
			if (note.lane < MIN_LANE || note.lane > MAX_LANE || maxWidth < 0 ||
			    note.width > maxWidth)
				return Result(ResultStatus::Error, "Note out of lane bounds");
		}

		for (const auto& [_, hold] : score.holdNotes)
		{
			if (hold.fadeType != FadeType::Out && hold.fadeType != FadeType::None &&
			    hold.fadeType != FadeType::Classic)
				return Result(ResultStatus::Error, "Unsupported fade type in SUS");

			if (holdIsGuide(hold))
			{
				const HoldNoteStep& sep = hold.separators.front();
				if (sep.guideColor != GuideColor::Green && sep.guideColor != GuideColor::Yellow)
					return Result(ResultStatus::Error,
					              "Only green/yellow guide colors supported in SUS");
			}

			for (const id_t stepID : hold.steps)
			{
				const Note& n = score.notes.at(stepID);
				if (n.ease > EaseType::EaseOut)
					return Result(ResultStatus::Error, "Unsupported ease type in SUS");
			}
		}

		return Result::Ok();
	}

	// -------------------------------------------------------------------------
	// susToScore
	// -------------------------------------------------------------------------

	SerializingScore SusSerializer::susToScore(const SUS& sus)
	{
		ScoreMetadata metadata;
		if (sus.metadata.data.count("title"))
			metadata.title = sus.metadata.data.at("title");
		if (sus.metadata.data.count("artist"))
			metadata.artist = sus.metadata.data.at("artist");
		if (sus.metadata.data.count("designer"))
			metadata.author = sus.metadata.data.at("designer");
		metadata.musicOffset = sus.metadata.waveOffset * 1000.0f;

		std::vector<std::string> hiSpeedGroupNames;
		for (const auto& group : sus.hiSpeedGroups)
			hiSpeedGroupNames.push_back(group.name);

		std::unordered_map<std::string, FlickType> flicks;
		std::unordered_set<std::string> criticals, stepIgnore, easeIns, easeOuts;
		std::unordered_set<std::string> slideKeys, frictions, hiddenHolds;

		for (const auto& slides : { sus.slides, sus.guides })
			for (const auto& slide : slides)
				for (const auto& note : slide)
					if (note.type == 1 || note.type == 2 || note.type == 3 || note.type == 5)
						slideKeys.insert(noteKey(note));

		for (const auto& dir : sus.directionals)
		{
			const std::string key = noteKey(dir);
			switch (dir.type)
			{
			case 1:
				flicks.insert_or_assign(key, FlickType::Default);
				break;
			case 3:
				flicks.insert_or_assign(key, FlickType::Left);
				break;
			case 4:
				flicks.insert_or_assign(key, FlickType::Right);
				break;
			case 2:
				easeIns.insert(key);
				break;
			case 5:
			case 6:
				easeOuts.insert(key);
				break;
			}
		}

		for (const auto& tap : sus.taps)
		{
			const std::string key = noteKey(tap);
			switch (tap.type)
			{
			case 2:
				criticals.insert(key);
				break;
			case 3:
				stepIgnore.insert(key);
				break;
			case 4:
				hiddenHolds.insert(key);
				break;
			case 5:
				frictions.insert(key);
				break;
			case 6:
				criticals.insert(key);
				frictions.insert(key);
				break;
			case 7:
				hiddenHolds.insert(key);
				break;
			case 8:
				hiddenHolds.insert(key);
				criticals.insert(key);
				break;
			}
		}

		NoteCollection notes;
		HoldNoteCollection holds;
		id_t nextNoteID = 0;
		id_t nextHoldID = 0;

		// --- Standalone notes ---
		for (const auto& tap : sus.taps)
		{
			if (!sus.sideLane && (tap.lane - 2 < MIN_LANE || tap.lane - 2 > MAX_LANE))
				continue;

			const std::string key = noteKey(tap);
			if (slideKeys.count(key))
				continue;

			Note n;
			if (tap.type == 4)
			{
				n.type = NoteType::Damage;
			}
			else
			{
				n.type = NoteType::Tap;
				if (criticals.count(key))
					n.flag = setFlag(n.flag, NoteFlag::Critical);
				if (frictions.count(key) || stepIgnore.count(key))
					n.flag = setFlag(n.flag, NoteFlag::Trace);
				n.flick = flicks.count(key) ? flicks[key] : FlickType::None;
			}
			n.tick = tap.tick;
			n.lane = (float)(tap.lane - 2);
			n.width = (float)tap.width;
			n.layer = (id_t)std::distance(
			    hiSpeedGroupNames.begin(),
			    std::find(hiSpeedGroupNames.begin(), hiSpeedGroupNames.end(), tap.hiSpeedGroup));
			n.ID = nextNoteID++;
			notes[n.ID] = n;
		}

		// --- Slides / Guides ---
		auto slideFillFunc = [&](const SUSNoteStream& slideStream, bool isGuideSlides)
		{
			for (const auto& slide : slideStream)
			{
				if (slide.size() < 2)
					continue;

				bool isGuide = isGuideSlides;
				const std::string key = noteKey(slide[0]);
				bool critical = criticals.count(key) > 0;

				HoldNote hold;
				hold.ID = nextHoldID;

				HoldNoteStep firstSep;
				firstSep.flag = isGuide ? HoldNoteFlag::Guide : HoldNoteFlag::Normal;
				if (critical)
					firstSep.flag = firstSep.flag | HoldNoteFlag::Critical;
				firstSep.guideColor = GuideColor::Green;

				for (const auto& susNote : slide)
				{
					const std::string nkey = noteKey(susNote);
					EaseType ease = EaseType::Linear;
					if (easeIns.count(nkey))
						ease = EaseType::EaseIn;
					if (easeOuts.count(nkey))
						ease = EaseType::EaseOut;

					switch (susNote.type)
					{
					case 1: // start
					{
						Note n;
						n.type = NoteType::Tap;
						n.tick = susNote.tick;
						n.lane = (float)(susNote.lane - 2);
						n.width = (float)susNote.width;
						n.layer = (id_t)std::distance(hiSpeedGroupNames.begin(),
						                              std::find(hiSpeedGroupNames.begin(),
						                                        hiSpeedGroupNames.end(),
						                                        susNote.hiSpeedGroup));
						n.holdID = nextHoldID;
						n.ID = nextNoteID++;
						n.ease = ease;
						if (critical)
							n.flag = setFlag(n.flag, NoteFlag::Critical);
						if (isGuide)
						{
							n.flag = setFlag(n.flag, NoteFlag::Hidden);
							firstSep.guideColor = critical ? GuideColor::Yellow : GuideColor::Green;
						}
						else
						{
							if (frictions.count(nkey) || stepIgnore.count(nkey))
								n.flag = setFlag(n.flag, NoteFlag::Trace);
							if (hiddenHolds.count(nkey))
								n.flag = setFlag(n.flag, NoteFlag::Hidden);
						}
						firstSep.ID = n.ID;
						notes[n.ID] = n;
						hold.steps.push_back(n.ID);
					}
					break;

					case 2: // end
					{
						Note n;
						n.type = NoteType::Tap;
						n.tick = susNote.tick;
						n.lane = (float)(susNote.lane - 2);
						n.width = (float)susNote.width;
						n.layer = (id_t)std::distance(hiSpeedGroupNames.begin(),
						                              std::find(hiSpeedGroupNames.begin(),
						                                        hiSpeedGroupNames.end(),
						                                        susNote.hiSpeedGroup));
						n.holdID = nextHoldID;
						n.ID = nextNoteID++;
						if (critical || criticals.count(nkey))
							n.flag = setFlag(n.flag, NoteFlag::Critical);
						if (isGuide)
						{
							n.flag = setFlag(n.flag, NoteFlag::Hidden);
							hold.fadeType =
							    hiddenHolds.count(nkey) ? FadeType::None : FadeType::Out;
						}
						else
						{
							n.flick = flicks.count(nkey) ? flicks[nkey] : FlickType::None;
							if (frictions.count(nkey) || stepIgnore.count(nkey))
								n.flag = setFlag(n.flag, NoteFlag::Trace);
							if (hiddenHolds.count(nkey))
								n.flag = setFlag(n.flag, NoteFlag::Hidden);
							hold.fadeType = FadeType::Out;
						}
						notes[n.ID] = n;
						hold.steps.push_back(n.ID);
					}
					break;

					case 3: // mid normal
					case 5: // mid hidden
					{
						Note n;
						n.type = NoteType::Tap;
						n.tick = susNote.tick;
						n.lane = (float)(susNote.lane - 2);
						n.width = (float)susNote.width;
						n.layer = (id_t)std::distance(hiSpeedGroupNames.begin(),
						                              std::find(hiSpeedGroupNames.begin(),
						                                        hiSpeedGroupNames.end(),
						                                        susNote.hiSpeedGroup));
						n.holdID = nextHoldID;
						n.ID = nextNoteID++;
						n.ease = ease;
						if (critical)
							n.flag = setFlag(n.flag, NoteFlag::Critical);
						if (susNote.type == 5)
							n.flag = setFlag(n.flag, NoteFlag::Hidden);
						if (stepIgnore.count(nkey))
							n.flag = setFlag(n.flag, NoteFlag::Attached);
						notes[n.ID] = n;
						hold.steps.push_back(n.ID);
					}
					break;
					}
				}

				if (hold.steps.size() < 2)
					continue;
				hold.separators.push_back(firstSep);
				hold.sortSteps(notes);
				holds[nextHoldID++] = hold;
			}
		};

		slideFillFunc(sus.slides, false);
		slideFillFunc(sus.guides, true);

		Score score;
		score.notes = std::move(notes);
		score.holdNotes = std::move(holds);

		if (sus.bpms.empty())
			score.tempoChanges[0] = Tempo{ 0, 120.0f };
		else
			for (const auto& bpm : sus.bpms)
				score.tempoChanges[bpm.tick] = Tempo{ bpm.tick, bpm.bpm };

		for (const auto& sign : sus.barlengths)
		{
			auto frac = barLengthToFraction(sign.length, 4.0f);
			score.timeSignatures[sign.bar] = TimeSignature{ sign.bar, frac.first, frac.second };
		}

		score.layers.clear();
		if (sus.hiSpeedGroups.empty())
		{
			score.layers.emplace_back("default");
		}
		else
		{
			for (int gi = 0; gi < (int)sus.hiSpeedGroups.size(); ++gi)
			{
				Layer layer(IO::formatString("#%d", gi));
				for (const auto& hs : sus.hiSpeedGroups[gi].hiSpeeds)
				{
					// Use MikuMikuWorld::HiSpeed (from ScoreEvents.h), not SUS::SusHiSpeed
					MikuMikuWorld::HiSpeed h;
					h.tick = hs.tick;
					h.layer = gi;
					h.speed = hs.speed;
					layer.hiSpeedChanges[hs.tick] = h;
				}
				score.layers.push_back(std::move(layer));
			}
		}

		return { score, metadata };
	}

	// -------------------------------------------------------------------------
	// scoreToSus
	// -------------------------------------------------------------------------

	SUS SusSerializer::scoreToSus(const SerializingScore& data)
	{
		const Score& score = data.score;
		const ScoreMetadata& metadata = data.metadata;

		constexpr int flickToType[] = { 0, 1, 3, 4, 0, 0, 0 };
		int offset = metadata.laneExtension == 0 ? 2 : metadata.laneExtension;

		std::vector<SUSNote> taps, directionals;
		std::vector<std::vector<SUSNote>> slides, guides;
		std::vector<BPM> bpms;
		std::vector<BarLength> barlengths;

		std::unordered_map<int, std::string> hiSpeedGroupNames;
		for (int i = 0; i < (int)score.layers.size(); ++i)
		{
			char b36[8]{};
			IO::tostringBaseN(b36, i, 36);
			std::string s(b36);
			if (s.size() == 1)
				s = "0" + s;
			hiSpeedGroupNames[i] = s;
		}

		std::unordered_set<std::string> criticalKeys;
		for (const auto& [_, note] : score.notes)
		{
			if (note.isHold())
				continue;

			if (note.type == NoteType::Tap)
			{
				int type = note.isTrace() ? 5 : 1;
				if (note.isCrit())
				{
					type++;
					criticalKeys.insert(noteKey(note));
				}
				taps.push_back(SUSNote{ note.tick, (int)note.lane + offset, (int)note.width, type,
				                        hiSpeedGroupNames[(int)note.layer] });
				if (note.isFlick() && note.flick < FlickType::Down)
					directionals.push_back(SUSNote{ note.tick, (int)note.lane + offset,
					                                (int)note.width,
					                                flickToType[(int)note.flick] });
			}
			else if (note.type == NoteType::Damage)
			{
				taps.push_back(SUSNote{ note.tick, (int)note.lane + offset, (int)note.width, 4,
				                        hiSpeedGroupNames[(int)note.layer] });
			}
		}

		std::vector<const HoldNote*> sortedHolds;
		sortedHolds.reserve(score.holdNotes.size());
		for (const auto& [_, hold] : score.holdNotes)
			sortedHolds.push_back(&hold);
		std::sort(sortedHolds.begin(), sortedHolds.end(),
		          [&score](const HoldNote* a, const HoldNote* b)
		          {
			          return score.notes.at(a->steps.front()).tick <
			                 score.notes.at(b->steps.front()).tick;
		          });

		for (const HoldNote* holdPtr : sortedHolds)
		{
			const HoldNote& hold = *holdPtr;
			if (hold.steps.size() < 2)
				continue;

			const Note& startNote = score.notes.at(hold.steps.front());
			const Note& endNote = score.notes.at(hold.steps.back());
			const bool isGuide = holdIsGuide(hold);
			const bool critical = startNote.isCrit();

			std::vector<SUSNote> slide;
			slide.reserve(hold.steps.size());

			slide.push_back(SUSNote{ startNote.tick, (int)startNote.lane + offset,
			                         (int)startNote.width, 1,
			                         hiSpeedGroupNames[(int)startNote.layer] });

			if (startNote.ease != EaseType::Linear)
				directionals.push_back(SUSNote{ startNote.tick, (int)startNote.lane + offset,
				                                (int)startNote.width,
				                                startNote.ease == EaseType::EaseIn ? 2 : 6 });

			bool guideAlreadyOnCritical = isGuide && criticalKeys.count(noteKey(startNote)) > 0;
			int startTapType = startNote.isHidden() ? 7 : startNote.isTrace() ? 5 : 1;
			bool isYellowGuide =
			    isGuide && hold.separators.front().guideColor == GuideColor::Yellow;
			if ((critical || isYellowGuide) && !criticalKeys.count(noteKey(startNote)))
			{
				startTapType++;
				criticalKeys.insert(noteKey(startNote));
			}
			if (startTapType != 1 &&
			    !((startTapType == 7 || startTapType == 8) && guideAlreadyOnCritical))
				taps.push_back({ startNote.tick, (int)startNote.lane + offset, (int)startNote.width,
				                 startTapType });

			for (int si = 1; si < (int)hold.steps.size() - 1; ++si)
			{
				const Note& mid = score.notes.at(hold.steps[si]);
				int susType = mid.isHidden() ? 5 : 3;
				slide.push_back(SUSNote{ mid.tick, (int)mid.lane + offset, (int)mid.width, susType,
				                         hiSpeedGroupNames[(int)mid.layer] });
				if (mid.isAttached())
					taps.push_back(SUSNote{ mid.tick, (int)mid.lane + offset, (int)mid.width, 3 });
				else if (mid.ease != EaseType::Linear)
				{
					int midTapType = isGuide ? (critical ? 8 : 7) : 1;
					taps.push_back(
					    SUSNote{ mid.tick, (int)mid.lane + offset, (int)mid.width, midTapType });
					directionals.push_back(SUSNote{
					    mid.tick, (int)mid.lane + offset, (int)mid.width,
					    mid.ease == EaseType::EaseIn ? 2 : 6, hiSpeedGroupNames[(int)mid.layer] });
				}
			}

			slide.push_back(
			    SUSNote{ endNote.tick, (int)endNote.lane + offset, (int)endNote.width, 2 });

			if (!isGuide && endNote.isFlick() && !endNote.isHidden() &&
			    endNote.flick < FlickType::Down)
				directionals.push_back(SUSNote{ endNote.tick, (int)endNote.lane + offset,
				                                (int)endNote.width,
				                                flickToType[(int)endNote.flick] });

			int endTapType = endNote.isHidden() ? 7 : endNote.isTrace() ? 5 : 1;
			if (endNote.isCrit())
			{
				endTapType++;
				criticalKeys.insert(noteKey(endNote));
			}
			if (hold.fadeType == FadeType::None)
				endTapType = 4;
			if (endTapType != 1 && endTapType != 2)
				taps.push_back(SUSNote{ endNote.tick, (int)endNote.lane + offset,
				                        (int)endNote.width, endTapType });

			if (isGuide)
				guides.push_back(slide);
			else
				slides.push_back(slide);
		}

		for (const auto& [tick, tempo] : score.tempoChanges)
			bpms.push_back(BPM{ tick, tempo.quarterPerMinute });
		if (bpms.empty())
			bpms.push_back(BPM{ 0, 120.0f });

		for (const auto& [_, ts] : score.timeSignatures)
			barlengths.push_back(
			    BarLength{ ts.measure, ((float)ts.numerator / (float)ts.denominator) * 4.0f });

		// Skill triggers: type 4, lane 0, width 1
		for (const auto& skill : score.skills)
			taps.push_back(SUSNote{ skill.tick, 0, 1, 4 });

		// Fever start/end
		if (score.fever.startTick != -1)
			taps.push_back(SUSNote{ score.fever.startTick, 15, 1, 1 });
		if (score.fever.endTick != -1)
			taps.push_back(SUSNote{ score.fever.endTick, 15, 1, 2 });

		// Build HiSpeedGroups using SusHiSpeed (SUS.h type)
		std::vector<HiSpeedGroup> hiSpeedGroups;
		for (int i = 0; i < (int)score.layers.size(); ++i)
		{
			HiSpeedGroup group;
			group.name = hiSpeedGroupNames[i];
			for (const auto& [tick, hs] : score.layers[i].hiSpeedChanges)
				group.hiSpeeds.push_back(SusHiSpeed{ tick, hs.speed });
			hiSpeedGroups.push_back(std::move(group));
		}

		SUSMetadata susMeta;
		susMeta.data["title"] = metadata.title;
		susMeta.data["artist"] = metadata.artist;
		susMeta.data["designer"] = metadata.author;
		susMeta.requests.push_back("ticks_per_beat 480");
		susMeta.requests.push_back("side_lane true");
		int laneOffset = metadata.laneExtension == 0 ? 0 : -metadata.laneExtension + 2;
		susMeta.requests.push_back("lane_offset " + std::to_string(laneOffset));
		susMeta.waveOffset = metadata.musicOffset / 1000.0f;

		return SUS{ susMeta, taps,       directionals,  slides,     guides,
			        bpms,    barlengths, hiSpeedGroups, laneOffset, true };
	}
}