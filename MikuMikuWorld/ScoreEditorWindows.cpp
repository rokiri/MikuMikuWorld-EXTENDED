#include "ScoreEditorWindows.h"
#include "Application.h"
#include "ApplicationConfiguration.h"
#include "Constants.h"
#include "File.h"
#include "NoteTypes.h"
#include "ScoreContext.h"
#include "UI.h"
#include "Utilities.h"

namespace MikuMikuWorld
{
	void ScorePropertiesWindow::update(ScoreContext& context)
	{
		if (ImGui::CollapsingHeader(
		        IO::concat(ICON_FA_ALIGN_LEFT, getString("metadata"), " ").c_str(),
		        ImGuiTreeNodeFlags_DefaultOpen))
		{
			UI::beginPropertyColumns();
			UI::addStringProperty(getString("title"), context.workingData.title);
			UI::addStringProperty(getString("designer"), context.workingData.designer);
			UI::addStringProperty(getString("artist"), context.workingData.artist);

			std::string jacketFile = context.workingData.jacket.getFilename();
			int result = UI::addFileProperty(getString("jacket"), jacketFile);
			if (result == 1)
			{
				context.workingData.jacket.load(jacketFile);
			}
			else if (result == 2)
			{
				IO::FileDialog fileDialog{};
				fileDialog.title = "Open Image File";
				fileDialog.filters = { IO::imageFilter, IO::allFilter };
				fileDialog.parentWindowHandle = Application::windowState.windowHandle;

				if (fileDialog.openFile() == IO::FileDialogResult::OK)
					context.workingData.jacket.load(fileDialog.outputFilename);
			}
			context.workingData.jacket.draw();

			UI::addIntProperty(getString("lane_extension"), context.workingData.laneExtension, 0,
			                   100);
			UI::endPropertyColumns();
		}

		if (ImGui::CollapsingHeader(IO::concat(ICON_FA_VOLUME_UP, getString("audio"), " ").c_str(),
		                            ImGuiTreeNodeFlags_DefaultOpen))
		{
			UI::beginPropertyColumns();

			bool changeMusic = false;
			std::string filename = context.workingData.musicFilename;
			int filePickResult = UI::addFileProperty(getString("music_file"), filename);
			if (filePickResult == 1 && filename != context.workingData.musicFilename)
			{
				isPendingLoadMusic = true;
				pendingLoadMusicFilename = filename;
			}
			else if (filePickResult == 2)
			{
				IO::FileDialog fileDialog{};
				fileDialog.title = "Open Audio File";
				fileDialog.filters = { IO::audioFilter, IO::allFilter };
				fileDialog.parentWindowHandle = Application::windowState.windowHandle;

				if (fileDialog.openFile() == IO::FileDialogResult::OK)
				{
					pendingLoadMusicFilename = fileDialog.outputFilename;
					isPendingLoadMusic = true;
				}
			}

			float offset = context.workingData.musicOffset;
			UI::addDragFloatProperty(getString("music_offset"), offset, "%.3fms");
			if (offset != context.workingData.musicOffset)
			{
				context.workingData.musicOffset = offset;
				context.audio.setMusicOffset(context.getTimeAtCurrentTick(), offset);
			}

			// volume controls
			float master = context.audio.getMasterVolume();
			float bgm = context.audio.getMusicVolume();
			float se = context.audio.getSoundEffectsVolume();

			UI::addPercentSliderProperty(getString("volume_master"), master);
			UI::addPercentSliderProperty(getString("volume_bgm"), bgm);
			UI::addPercentSliderProperty(getString("volume_se"), se);
			UI::endPropertyColumns();

			if (master != context.audio.getMasterVolume())
				context.audio.setMasterVolume(master);

			if (bgm != context.audio.getMusicVolume())
				context.audio.setMusicVolume(bgm);

			if (se != context.audio.getSoundEffectsVolume())
				context.audio.setSoundEffectsVolume(se);
		}

		if (ImGui::CollapsingHeader(
		        IO::concat(ICON_FA_CHART_BAR, getString("statistics"), " ").c_str(),
		        ImGuiTreeNodeFlags_DefaultOpen))
		{
			UI::beginPropertyColumns();
			UI::addReadOnlyProperty(getString("hispeeds"), context.scoreStats.getHiSpeeds());
			UI::addReadOnlyProperty(getString("taps"), context.scoreStats.getTaps());
			UI::addReadOnlyProperty(getString("flicks"), context.scoreStats.getFlicks());
			UI::addReadOnlyProperty(getString("holds"), context.scoreStats.getHolds());
			UI::addReadOnlyProperty(getString("steps"), context.scoreStats.getSteps());
			UI::addReadOnlyProperty(getString("guides"), context.scoreStats.getGuides());
			UI::addReadOnlyProperty(getString("traces"), context.scoreStats.getTraces());
			UI::addReadOnlyProperty(getString("total"), context.scoreStats.getTotal());
			UI::addReadOnlyProperty(getString("combo"), context.scoreStats.getCombo());
			UI::endPropertyColumns();
		}
	}

	void ScoreNotePropertiesWindow::update(ScoreContext& context, int currentDivision)
	{
		auto numSelected = context.selectedNotes.size() + context.selectedHiSpeedChanges.size();
		if (numSelected == 0)
		{
			ImGui::Text("%s", getString("note_properties_not_selected"));
			return;
		}

		Score prev = context.score;
		bool edited = false;

		int selectedTick;
		int selectedLayer;
		try
		{
			selectedTick =
			    context.selectedNotes.size() >= 1
			        ? context.score.notes.at(*context.selectedNotes.begin()).tick
			        : context.score.hiSpeedChanges.at(*context.selectedHiSpeedChanges.begin()).tick;
			selectedLayer =
			    context.selectedNotes.size() >= 1
			        ? context.score.notes.at(*context.selectedNotes.begin()).layer
			        : context.score.hiSpeedChanges.at(*context.selectedHiSpeedChanges.begin())
			              .layer;
		}
		catch (const std::out_of_range& e)
		{
			ImGui::Text("%s", getString("note_properties_not_selected"));
			return;
		}

		if (ImGui::CollapsingHeader(IO::concat(ICON_FA_COG, getString("general"), " ").c_str(),
		                            ImGuiTreeNodeFlags_DefaultOpen))
		{
			UI::beginPropertyColumns();

			double beat = selectedTick / static_cast<float>(TICKS_PER_BEAT);
			
			UI::propertyLabel(getString("beat"));
			ImGui::SetNextItemWidth(-1);
			
			// MMWでは4分音符=1拍(Beat)として扱うため、4.0 / Division で1ステップの拍数を計算
			double step = 4.0 / (currentDivision > 0 ? currentDivision : 4);
			double step_fast = step * 4.0; // Shiftキーを押しながらの高速移動用
			
			bool beatChanged = ImGui::InputDouble(IO::concat("##", getString("beat")).c_str(), &beat, step, step_fast, "%.3f");
			ImGui::NextColumn();

			if (beatChanged)
			{
				// 浮動小数点数の微細な誤差を防ぐため floor から round に変更
				auto newTick = std::round(beat * TICKS_PER_BEAT); 
				for (auto& id : context.selectedNotes)
				{
					context.score.notes.at(id).tick = newTick;
				}
				for (auto& id : context.selectedHiSpeedChanges)
				{
					context.score.hiSpeedChanges.at(id).tick = newTick;
				}
				edited = true;
			}

			if (config.showTickInProperties)
			{
				if (UI::addIntProperty(getString("tick"), selectedTick))
				{
					for (auto& id : context.selectedNotes)
					{
						context.score.notes.at(id).tick = selectedTick;
					}
					for (auto& id : context.selectedHiSpeedChanges)
					{
						context.score.hiSpeedChanges.at(id).tick = selectedTick;
					}
					edited = true;
				}
			}

			UI::propertyLabel(getString("layer"));
			const std::string layer_name = context.score.layers[selectedLayer].name;
			if (ImGui::BeginCombo(IO::concat("##", getString("layer")).c_str(), layer_name.c_str()))
			{
				for (int i = 0; i < context.score.layers.size(); i++)
				{
					auto& layer = context.score.layers[i];
					bool selected = selectedLayer == i;
					if (ImGui::Selectable(layer.name.c_str(), selected))
					{
						for (auto& id : context.selectedNotes)
						{
							context.score.notes.at(id).layer = i;
						}
						for (auto& id : context.selectedHiSpeedChanges)
						{
							context.score.hiSpeedChanges.at(id).layer = i;
						}
						edited = true;
					}
				}
				ImGui::EndCombo();
			}

			UI::endPropertyColumns();
		}

		if (context.selectedNotes.size() >= 1)
		{
			bool isGuide = false;

			bool multipleHold = false;
			int holdIndex = -1;
			for (id_t id : context.selectedNotes)
			{
				const Note& n = context.score.notes.at(id);
				if (n.isHold())
				{
					auto prevHoldIndex = holdIndex;
					if (n.getType() == NoteType::Hold)
					{
						holdIndex = id;
					}
					else
					{
						holdIndex = n.parentID;
					}
					if (prevHoldIndex != -1 && prevHoldIndex != holdIndex)
					{
						multipleHold = true;
						break;
					}
				}
			}

			Note& note = context.score.notes.at(*context.selectedNotes.begin());
			if (ImGui::CollapsingHeader(
			        IO::concat(ICON_FA_COG, getString("note_properties_note"), " ").c_str(),
			        ImGuiTreeNodeFlags_DefaultOpen))
			{
				UI::beginPropertyColumns();

				if (UI::addFloatProperty(getString("lane"), note.lane, "%.2f"))
				{
					for (auto& id : context.selectedNotes)
					{
						context.score.notes.at(id).lane = note.lane;
					}
					edited = true;
				}

				if (UI::addFloatProperty(getString("width"), note.width, "%.2f"))
				{
					for (auto& id : context.selectedNotes)
					{
						auto& localNote = context.score.notes.at(id);
						if (localNote.isHold())
						{
							auto& hold = context.score.holdNotes.at(
							    localNote.parentID == -1 ? id : localNote.parentID);
							if (!((localNote.getType() == NoteType::Hold &&
							       hold.startType == HoldNoteType::Normal) ||
							      (localNote.getType() == NoteType::HoldMid &&
							       hold.steps.at(findHoldStep(hold, localNote.ID)).type ==
							           HoldStepType::Normal) ||
							      (localNote.getType() == NoteType::HoldEnd &&
							       hold.endType == HoldNoteType::Normal)))
							{
								localNote.width = std::max(0.0f, note.width);
								continue;
							}
						}

						localNote.width = std::max(0.5f, note.width);
					}
					edited = true;
				}
			}

			if (context.hasHoldInSelection())
			{
				if (!multipleHold)
				{
					auto& hold = context.score.holdNotes.at(holdIndex);
					auto& holdStart = context.score.notes.at(hold.start.ID);

					isGuide = hold.isGuide();
					if (note.width == 0 && !isGuide)
					{
						if (note.getType() == NoteType::Hold)
						{
							hold.startType = HoldNoteType::Hidden;
						}
						else if (note.getType() == NoteType::HoldEnd)
						{
							hold.endType = HoldNoteType::Hidden;
						}
					}

					if (!isGuide)
					{
						if (note.getType() == NoteType::Hold || note.getType() == NoteType::HoldEnd)
						{
							if (UI::addCheckboxProperty(getString("trace"), note.friction))
							{
								for (auto& id : context.selectedNotes)
								{
									auto& n = context.score.notes.at(id);
									if (n.canTrace())
									{
										n.friction = note.friction;
									}
								}
								edited = true;
							}
						}
						if (UI::addCheckboxProperty(getString("critical"), note.critical))
						{
							context.score.notes.at(hold.start.ID).critical = note.critical;
							for (auto& step : hold.steps)
							{
								context.score.notes.at(step.ID).critical = note.critical;
							}
							context.score.notes.at(hold.end).critical = note.critical;
							for (auto& id : context.selectedNotes)
							{
								context.score.notes.at(id).critical = note.critical;
							}
							edited = true;
						}

						bool selectingAnyDummy = !std::all_of(
						    context.selectedNotes.begin(), context.selectedNotes.end(),
						    [&](id_t id)
						    {
							    auto& n = context.score.notes.at(id);
							    if (!n.isHold())
								    return false;
							    auto& h = context.score.holdNotes.at(
							        n.getType() == NoteType::Hold ? n.ID : n.parentID);
							    switch (n.getType())
							    {
							    case NoteType::Hold:
								    return h.startType == HoldNoteType::Hidden;
							    case NoteType::HoldEnd:
								    return h.endType == HoldNoteType::Hidden;
							    case NoteType::HoldMid:
								    return h[findHoldStep(h, id)].type == HoldStepType::Hidden;
							    }
							    return false;
						    });

						if (selectingAnyDummy &&
						    UI::addCheckboxProperty(getString("dummy"), note.dummy))
						{
							for (auto& id : context.selectedNotes)
							{
								auto& n = context.score.notes.at(id);
								n.dummy = note.dummy;
							}
							edited = true;
						}
					}

					if (note.getType() == NoteType::HoldEnd && !isGuide)
					{
						if (UI::addFlickSelectPropertyWithNone(getString("flick"), note.flick,
						                                       flickTypes, arrayLength(flickTypes)))
						{
							context.score.notes.at(hold.start.ID).flick = note.flick;
							for (auto& id : context.selectedNotes)
							{
								auto& n = context.score.notes.at(id);
								if (!n.isHold())
								{
									n.flick = note.flick;
								}
							}
						}
					}
				}
			}
			else
			{
				if (UI::addCheckboxProperty(getString("trace"), note.friction))
				{
					for (auto& id : context.selectedNotes)
					{
						auto& n = context.score.notes.at(id);
						if (n.canTrace())
						{
							n.friction = note.friction;
						}
					}
					edited = true;
				}
				if (UI::addCheckboxProperty(getString("critical"), note.critical))
				{
					for (auto& id : context.selectedNotes)
					{
						context.score.notes.at(id).critical = note.critical;
					}
					edited = true;
				}
				if (UI::addFlickSelectPropertyWithNone(getString("flick"), note.flick, flickTypes,
				                                       arrayLength(flickTypes)))
				{
					for (auto& id : context.selectedNotes)
					{
						auto& n = context.score.notes.at(id);
						if (n.canFlick())
						{
							n.flick = note.flick;
						}
					}
					edited = true;
				}

				if (UI::addCheckboxProperty(getString("dummy"), note.dummy))
				{
					for (auto& id : context.selectedNotes)
					{
						auto& n = context.score.notes.at(id);
						n.dummy = note.dummy;
					}
					edited = true;
				}
			}

			UI::endPropertyColumns();
			if (context.hasHoldInSelection())
			{
				if (ImGui::CollapsingHeader(IO::concat(ICON_FA_COG,
				                                       isGuide
				                                           ? getString("note_properties_guide")
				                                           : getString("note_properties_hold_note"),
				                                       " ")
				                                .c_str(),
				                            ImGuiTreeNodeFlags_DefaultOpen))
				{

					if (multipleHold)
					{
						ImGui::Text("%s", getString("note_properties_many_hold_notes"));
					}
					else
					{
						auto& hold = context.score.holdNotes.at(holdIndex);
						auto& holdStart = context.score.notes.at(hold.start.ID);

						int stepIndex = findHoldStep(hold, note.ID);

						UI::beginPropertyColumns();

						bool hasEaseable = false;
						Note easeableNote;
						bool hasStepType = false;
						Note stepTypeNote;
						for (auto id : context.selectedNotes)
						{
							auto& n = context.score.notes.at(id);
							if (n.hasEase())
							{
								if (!hasEaseable)
								{
									easeableNote = n;
								}
								hasEaseable = true;
							}
							if (context.score.notes.at(id).getType() == NoteType::HoldMid)
							{
								if (!hasStepType)
								{
									stepTypeNote = n;
								}
								hasStepType = true;
							}
						}

						if (hasEaseable)
						{
							int stepIndex = findHoldStep(hold, easeableNote.ID);
							auto ease = easeableNote.getType() == NoteType::Hold
							                ? hold.start.ease
							                : hold.steps.at(stepIndex == -1 ? 0 : stepIndex).ease;
							if (UI::addSelectProperty(getString("ease_type"), ease, easeTypes,
							                          arrayLength(easeTypes)))
							{
								for (auto id : context.selectedNotes)
								{
									auto& note = context.score.notes.at(id);
									if (note.hasEase())
									{
										if (note.getType() == NoteType::Hold)
										{
											hold.start.ease = ease;
										}
										else
										{
											auto& step = hold.steps.at(findHoldStep(hold, note.ID));
											step.ease = ease;
										}
									}
								}
							}
						}

						if (hasStepType)
						{
							int stepIndex = findHoldStep(hold, stepTypeNote.ID);
							auto stepType = hold.steps.at(stepIndex).type;

							if (UI::addSelectProperty(getString("step_type"), stepType, stepTypes,
							                          arrayLength(stepTypes)))
							{
								for (auto id : context.selectedNotes)
								{
									auto& note = context.score.notes.at(id);
									if (note.getType() == NoteType::HoldMid)
									{
										auto& step = hold.steps.at(findHoldStep(hold, note.ID));
										step.type = stepType;
									}
								}
							}
						}

						if (isGuide)
						{
							edited |= UI::addSelectProperty(getString("guide_color"),
							                                hold.guideColor, guideColorsForString,
							                                arrayLength(guideColors));
							edited |= UI::addSelectProperty(getString("fade_type"), hold.fadeType,
							                                fadeTypes, arrayLength(fadeTypes));
						}
						else
						{
							auto holdType =
							    note.getType() == NoteType::Hold ? hold.startType : hold.endType;
							if (UI::addSelectProperty(getString("hold_type"), holdType, holdTypes,
							                          2))
							{
								for (auto id : context.selectedNotes)
								{
									auto& note = context.score.notes.at(id);
									if (note.getType() == NoteType::Hold)
									{
										hold.startType = holdType;
									}
									else
									{
										hold.endType = holdType;
									}
								}
								edited = true;
							}

							if (UI::addCheckboxProperty(getString("hold_dummy"), hold.dummy))
							{
								edited = true;
							}
						}
					}
				}

				UI::endPropertyColumns();
			}
		}
		if (context.selectedHiSpeedChanges.size() >= 1)
		{
			bool speedEdited = false;
			HiSpeedChange& editHiSpeed =
			    context.score.hiSpeedChanges.at(*context.selectedHiSpeedChanges.begin());
			float speed = editHiSpeed.speed;
			float skip = editHiSpeed.skips;
			auto ease = editHiSpeed.ease;
			bool hideNotes = editHiSpeed.hideNotes;
			if (ImGui::CollapsingHeader(
			        IO::concat(ICON_FA_FAST_FORWARD, getString("note_properties_hi_speed"), " ")
			            .c_str(),
			        ImGuiTreeNodeFlags_DefaultOpen))
			{
				UI::beginPropertyColumns();
				speedEdited |= UI::addFloatProperty(getString("hi_speed_speed"), speed, "%.3f");
				speedEdited |=
				    UI::addSelectProperty(getString("hi_speed_ease"), ease, hiSpeedEaseNames,
				                          arrayLength(hiSpeedEaseNames));

				speedEdited |=
				    UI::addFloatProperty(getString("hi_speed_skip_beat"), skip, 
					IO::formatString("%%.3f %s", getString("beat")).c_str());

				speedEdited |= UI::addCheckboxProperty(getString("hi_speed_hide_notes"), hideNotes);
				UI::endPropertyColumns();
			}

			if (speedEdited)
			{
				edited = true;
				for (auto& id : context.selectedHiSpeedChanges)
				{
					HiSpeedChange& hiSpeed = context.score.hiSpeedChanges.at(id);
					hiSpeed.speed = speed;
					hiSpeed.skips = skip;
					hiSpeed.ease = ease;
					hiSpeed.hideNotes = hideNotes;
				}
			}
		}

		if (edited)
			context.pushHistory("Edited object", prev, context.score);
	}

	void ScoreOptionsWindow::update(ScoreContext& context, EditArgs& edit, TimelineMode currentMode)
	{
		UI::beginPropertyColumns();
		switch (currentMode)
		{
		default:
		case TimelineMode::InsertTap:
		case TimelineMode::InsertDamage:
		case TimelineMode::MakeCritical:
		case TimelineMode::MakeFriction:
		case TimelineMode::MakeDummy:
			UI::addIntProperty(getString("note_width"), edit.noteWidth, MIN_NOTE_WIDTH,
			                   MAX_NOTE_WIDTH);
			break;
		case TimelineMode::InsertFlick:
			UI::addIntProperty(getString("note_width"), edit.noteWidth, MIN_NOTE_WIDTH,
			                   MAX_NOTE_WIDTH);
			UI::addSelectProperty<FlickType>(getString("flick"), edit.flickType, flickTypes,
			                                 arrayLength(flickTypes));
			break;
		case TimelineMode::InsertLong:
			UI::addIntProperty(getString("note_width"), edit.noteWidth, MIN_NOTE_WIDTH,
			                   MAX_NOTE_WIDTH);
			UI::addSelectProperty<HoldEndType>(getString("hold_start_type"), edit.holdStartType,
			                                   holdEndTypes, arrayLength(holdEndTypes));
			UI::addSelectProperty<HoldEndType>(getString("hold_end_type"), edit.holdEndType,
			                                   holdEndTypes, arrayLength(holdEndTypes));
			UI::addSelectProperty(getString("ease_type"), edit.easeType, easeTypes,
			                      arrayLength(easeTypes));
			break;
		case TimelineMode::InsertLongMid:
			UI::addIntProperty(getString("note_width"), edit.noteWidth, MIN_NOTE_WIDTH,
			                   MAX_NOTE_WIDTH);
			UI::addSelectProperty(getString("step_type"), edit.stepType, stepTypes,
			                      arrayLength(stepTypes));
			break;
		case TimelineMode::InsertGuide:
			UI::addIntProperty(getString("note_width"), edit.noteWidth, MIN_NOTE_WIDTH,
			                   MAX_NOTE_WIDTH);
			UI::addSelectProperty(getString("ease_type"), edit.easeType, easeTypes,
			                      arrayLength(easeTypes));
			UI::addSelectProperty<GuideColor>(getString("guide_color"), edit.colorType,
			                                  guideColorsForString, arrayLength(guideColors));
			UI::addSelectProperty<FadeType>(getString("fade_type"), edit.fadeType, fadeTypes,
			                                arrayLength(fadeTypes));
			break;
		case TimelineMode::InsertBPM:
			UI::addFloatProperty(getString("bpm"), edit.bpm, "%g BPM");
			edit.bpm = std::clamp(edit.bpm, MIN_BPM, MAX_BPM);
			break;

		case TimelineMode::InsertTimeSign:
			UI::timeSignatureSelect(edit.timeSignatureNumerator, edit.timeSignatureDenominator);
			break;

		case TimelineMode::InsertHiSpeed:
			UI::addFloatProperty(getString("hi_speed_speed"), edit.hiSpeed, "%gx");
			UI::addSelectProperty(getString("hi_speed_ease"), edit.hiSpeedEase, hiSpeedEaseNames,
			                      arrayLength(hiSpeedEaseNames));
			UI::addFloatProperty(getString("hi_speed_skip_beat"), edit.hiSpeedSkip, IO::formatString("%%.3f %s", getString("beat")).c_str());
			UI::addCheckboxProperty(getString("hi_speed_hide_notes"), edit.hiSpeedHideNotes);
			break;
		}
		UI::endPropertyColumns();
	}

	void PresetsWindow::update(ScoreContext& context, PresetManager& presetManager)
	{
		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_DRAFTING_COMPASS, "presets")))
		{
			int removePattern = -1;

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
			                    ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
			float filterWidth = ImGui::GetContentRegionAvail().x - UI::btnSmall.x - 2;

			presetFilter.Draw("##preset_filter",
			                  IO::concat(ICON_FA_SEARCH, getString("search"), " ").c_str(),
			                  filterWidth);
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TIMES, UI::btnSmall))
				presetFilter.Clear();

			ImGui::PopStyleVar();
			ImGui::PopStyleColor();

			float presetButtonHeight = ImGui::GetFrameHeight();
			float windowHeight = ImGui::GetContentRegionAvail().y - presetButtonHeight -
			                     ImGui::GetStyle().WindowPadding.y;
			if (ImGui::BeginChild("presets_child_window", ImVec2(-1, windowHeight), true))
			{
				if (!presetManager.presets.size())
				{
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
					ImGui::TextWrapped("%s", getString("no_presets"));
					ImGui::PopStyleVar();
				}
				else
				{
					for (const auto& [id, preset] : presetManager.presets)
					{
						if (!presetFilter.PassFilter(preset.getName().c_str()))
							continue;

						ImGui::PushID(id);

						if (ImGui::Button(
						        preset.getName().c_str(),
						        ImVec2(ImGui::GetContentRegionAvail().x - UI::btnSmall.x - 2.0f,
						               presetButtonHeight)))
							presetManager.applyPreset(id, context);

						if (preset.description.size())
							UI::tooltip(preset.description.c_str());

						ImGui::SameLine();
						if (UI::transparentButton(ICON_FA_TRASH,
						                          ImVec2(UI::btnSmall.x, presetButtonHeight)))
							removePattern = id;

						ImGui::PopID();
					}
				}
			}
			ImGui::EndChild();
			ImGui::Separator();

			const bool disable =
			    !(context.selectedNotes.size() + context.selectedHiSpeedChanges.size());
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, disable);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1 - (0.5f * disable));
			if (ImGui::Button(getString("create_preset"), ImVec2(-1, presetButtonHeight)))
				dialogOpen = true;

			ImGui::PopStyleVar();
			ImGui::PopItemFlag();

			if (removePattern != -1)
				presetManager.removePreset(removePattern);
		}

		ImGui::End();

		if (dialogOpen)
		{
			ImGui::OpenPopup(MODAL_TITLE("create_preset"));
			dialogOpen = false;
		}

		if (updateCreationDialog() == DialogResult::Ok)
		{
			presetManager.createPreset(context.score, context.selectedNotes,
			                           context.selectedHiSpeedChanges, presetName, presetDesc);
			presetName.clear();
			presetDesc.clear();
		}
	}

	DialogResult PresetsWindow::updateCreationDialog()
	{
		DialogResult result = DialogResult::None;

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always,
		                        ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Always);
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		if (ImGui::BeginPopupModal(MODAL_TITLE("create_preset"), NULL, ImGuiWindowFlags_NoResize))
		{
			ImVec2 padding = ImGui::GetStyle().WindowPadding;
			ImVec2 spacing = ImGui::GetStyle().ItemSpacing;

			float xPos = padding.x;
			float yPos = ImGui::GetWindowSize().y - UI::btnSmall.y - 2.0f - (padding.y * 2);

			ImGui::Text("%s", getString("preset_name"));
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##preset_name", &presetName);

			ImGui::Text("%s", getString("description"));
			ImGui::InputTextMultiline(
			    "##preset_desc", &presetDesc,
			    { -1, ImGui::GetContentRegionAvail().y - UI::btnSmall.y - 10.0f - padding.y });

			ImVec2 btnSz{ (ImGui::GetContentRegionAvail().x - spacing.x - (padding.x * 0.5f)) /
				              2.0f,
				          ImGui::GetFrameHeight() };
			ImGui::SetCursorPos(ImVec2(xPos, yPos));
			if (ImGui::Button(getString("cancel"), btnSz))
			{
				result = DialogResult::Cancel;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !presetName.size());
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1 - (0.5f * !presetName.size()));
			if (ImGui::Button(getString("confirm"), btnSz))
			{
				result = DialogResult::Ok;
				ImGui::CloseCurrentPopup();
			}

			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
			ImGui::EndPopup();
		}

		return result;
	}

	DialogResult RecentFileNotFoundDialog::update()
	{
		DialogResult result{ DialogResult::None };
		if (open)
		{
			ImGui::OpenPopup(MODAL_TITLE("file_not_found"));
			open = false;
		}

		std::string dialogText = IO::formatString(
		    "%s \"%s\" %s. %s", getString("file_not_found_msg1"), removeFilename.c_str(),
		    getString("file_not_found_msg2"), getString("remove_recent_file_not_found"));

		float maxDialogSizeX{ ImGui::GetMainViewport()->WorkSize.x * 0.80f };
		ImVec2 padding = ImGui::GetStyle().WindowPadding;
		ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
		ImVec2 textSize = ImGui::CalcTextSize(dialogText.c_str());

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always,
		                        ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(
		    ImVec2(std::min(maxDialogSizeX, textSize.x + (padding.x * 2) + spacing.x), 0),
		    ImGuiCond_Always);
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		if (ImGui::BeginPopupModal(MODAL_TITLE("file_not_found"), NULL, ImGuiWindowFlags_NoResize))
		{
			ImGui::TextWrapped("%s", dialogText.c_str());

			// New line to move the buttons a bit down
			ImGui::Text("\n");

			ImVec2 btnSz{ (ImGui::GetContentRegionAvail().x - spacing.x - (padding.x * 0.5f)) /
				              2.0f,
				          ImGui::GetFrameHeight() };
			btnSz.x = std::min(btnSz.x, 150.0f);

			// Right align buttons
			ImGui::SetCursorPos(
			    ImVec2(ImGui::GetWindowSize().x - (btnSz.x * 2) - spacing.x - padding.x,
			           ImGui::GetCursorPosY()));

			if (ImGui::Button(getString("yes"), btnSz))
			{
				close();
				result = DialogResult::Yes;
			}

			ImGui::SameLine();
			if (ImGui::Button(getString("no"), btnSz))
			{
				close();
				result = DialogResult::No;
			}

			ImGui::EndPopup();
		}

		return result;
	}

	DialogResult UnsavedChangesDialog::update()
	{
		DialogResult result = DialogResult::None;
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always,
		                        ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(450, 200), ImGuiCond_Always);
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		if (ImGui::BeginPopupModal(MODAL_TITLE("unsaved_changes"), NULL, ImGuiWindowFlags_NoResize))
		{
			ImGui::Text("%s", getString("ask_save"));
			ImGui::Text("%s", getString("warn_unsaved"));

			ImVec2 padding = ImGui::GetStyle().WindowPadding;
			ImVec2 spacing = ImGui::GetStyle().ItemSpacing;

			float btnsHeight = ImGui::GetFrameHeight();
			float xPos = padding.x;
			float yPos = ImGui::GetWindowSize().y - btnsHeight - padding.y;
			ImGui::SetCursorPos(ImVec2(xPos, yPos));

			ImVec2 btnSz{ (ImGui::GetContentRegionAvail().x - spacing.x - (padding.x * 0.5f)) /
				              3.0f,
				          btnsHeight };

			if (ImGui::Button(getString("save_changes"), btnSz))
			{
				close();
				result = DialogResult::Yes;
			}

			ImGui::SameLine();
			if (ImGui::Button(getString("discard_changes"), btnSz))
			{
				close();
				result = DialogResult::No;
			}

			ImGui::SameLine();
			if (ImGui::Button(getString("cancel"), btnSz))
			{
				close();
				result = DialogResult::Cancel;
			}

			ImGui::EndPopup();
		}

		return result;
	}

	DialogResult UpdateAvailableDialog::update()
	{
		if (open)
		{
			ImGui::OpenPopup(MODAL_TITLE("update_available"));
			open = false;
		}

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always,
		                        ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(450, 250), ImGuiCond_Always);
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		if (ImGui::BeginPopupModal(MODAL_TITLE("update_available"), NULL,
		                           ImGuiWindowFlags_NoResize))
		{
			ImGui::Text(getString("update_available_description"), latestVersion.c_str());

			float okButtonHeight = ImGui::GetFrameHeight();

			ImGui::SetCursorPos(
			    { ImGui::GetStyle().WindowPadding.x,
			      ImGui::GetWindowSize().y - okButtonHeight - ImGui::GetStyle().WindowPadding.y });

			ImVec2 padding = ImGui::GetStyle().WindowPadding;
			ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
			ImVec2 btnSz{ (ImGui::GetContentRegionAvail().x - spacing.x - (padding.x * 0.5f)) /
				              2.0f,
				          okButtonHeight };

			if (ImGui::Button(getString("yes"), btnSz))
			{
				system("start https://github.com/UntitledCharts/MikuMikuWorld4UC/releases");
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return DialogResult::Ok;
			}

			ImGui::SameLine();
			if (ImGui::Button(getString("no"), btnSz))
			{
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return DialogResult::Ok;
			}

			ImGui::EndPopup();
		}

		return DialogResult::None;
	}

	DialogResult AboutDialog::update()
	{
		if (open)
		{
			ImGui::OpenPopup(MODAL_TITLE("about"));
			open = false;
		}

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always,
		                        ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_Appearing);
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		if (ImGui::BeginPopupModal(MODAL_TITLE("about"), NULL, ImGuiWindowFlags_NoResize))
		{
			ImGui::Text(APP_NAME "\n"
			                     "This application is based on MikuMikuWorld Extended.\n\n"
			                     "See LICENSE and NOTICE for license and attribution information.\n\n");
			ImGui::Separator();

			float okButtonHeight = ImGui::GetFrameHeight();

			ImGui::Text("Version %s", Application::getAppVersion().c_str());

			ImGui::SetCursorPos(
			    { ImGui::GetStyle().WindowPadding.x,
			      ImGui::GetWindowSize().y - okButtonHeight - ImGui::GetStyle().WindowPadding.y });

			if (ImGui::Button("OK", { ImGui::GetContentRegionAvail().x, okButtonHeight }))
			{
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return DialogResult::Ok;
			}

			ImGui::EndPopup();
		}

		return DialogResult::None;
	}

	void DebugWindow::update(ScoreContext& context, ScoreEditorTimeline& timeline)
	{
		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_BUG, "debug")))
		{
			constexpr ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_DefaultOpen;
			constexpr ImGuiTreeNodeFlags treeNodeFlags = headerFlags | ImGuiTreeNodeFlags_Framed;
			if (ImGui::TreeNodeEx("Audio", treeNodeFlags))
			{
				if (ImGui::CollapsingHeader("Engine", headerFlags))
				{
					UI::beginPropertyColumns();
					UI::addReadOnlyProperty("Sample Rate", context.audio.getDeviceSampleRate());
					UI::addReadOnlyProperty("Channel Count", context.audio.getDeviceChannelCount());
					UI::addReadOnlyProperty(
					    "Latency",
					    IO::formatString("%.2fms", context.audio.getDeviceLatency() * 1000));
					UI::endPropertyColumns();
				}

				if (ImGui::CollapsingHeader("Music", headerFlags))
				{
					UI::beginPropertyColumns();
					UI::addReadOnlyProperty("Music Initialized",
					                        boolToString(context.audio.isMusicInitialized()));
					UI::addReadOnlyProperty("Music Filename", context.audio.musicBuffer.name);

					float musicTime = context.audio.getMusicPosition(),
					      musicLength = context.audio.getMusicLength();
					int musicTimeSeconds = static_cast<int>(musicTime),
					    musicLengthSeconds = static_cast<int>(musicLength);
					UI::addReadOnlyProperty(
					    "Music Time",
					    IO::formatString(
					        "%02d:%02d/%02d:%02d", musicTimeSeconds,
					        static_cast<int>((musicTime - musicTimeSeconds) * 100),
					        musicLengthSeconds,
					        static_cast<int>((musicLength - musicLengthSeconds) * 100)));

					UI::addReadOnlyProperty("Sample Rate", context.audio.musicBuffer.sampleRate);
					UI::addReadOnlyProperty("Effective Sample Rate",
					                        context.audio.musicBuffer.effectiveSampleRate);
					UI::addReadOnlyProperty("Channel Count",
					                        context.audio.musicBuffer.channelCount);
					UI::endPropertyColumns();
				}

				if (ImGui::CollapsingHeader("Waveform", headerFlags))
				{
					UI::beginPropertyColumns();
					UI::addReadOnlyProperty("Waveform L",
					                        boolToString(!context.waveformL.isEmpty()));
					UI::addReadOnlyProperty("Waveform L Mip Count",
					                        context.waveformL.getUsedMipCount());
					UI::addReadOnlyProperty("Waveform L Samples",
					                        context.waveformL.mips->absoluteSamples.size());
					UI::addReadOnlyProperty("Waveform R",
					                        boolToString(!context.waveformR.isEmpty()));
					UI::addReadOnlyProperty("Waveform R Mip Count",
					                        context.waveformR.getUsedMipCount());
					UI::addReadOnlyProperty("Waveform R Samples",
					                        context.waveformL.mips->absoluteSamples.size());
					UI::endPropertyColumns();

					if (ImGui::Button("Re-Generate Waveform", { -1, UI::btnSmall.y }))
					{
						context.waveformL.generateMipChainsFromSampleBuffer(
						    context.audio.musicBuffer, 0);
						context.waveformR.generateMipChainsFromSampleBuffer(
						    context.audio.musicBuffer, 1);
					}
				}

				if (ImGui::CollapsingHeader("Sound Test", headerFlags))
				{
					constexpr ImGuiTableFlags tableFlags =
					    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH |
					    ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
					    ImGuiTableFlags_RowBg;

					const int rowHeight = ImGui::GetFrameHeight() + 5;

					if (ImGui::BeginTable("##sound_test_table", 3, tableFlags, { -1, 200 }))
					{
						ImGui::TableSetupScrollFreeze(0, 1);
						ImGui::TableSetupColumn("Name");
						ImGui::TableSetupColumn("Duration (sec)", ImGuiTableColumnFlags_WidthFixed);
						ImGui::TableSetupColumn("Play/Stop", ImGuiTableColumnFlags_WidthFixed);
						ImGui::TableHeadersRow();

						for (size_t i = 0;
						     i < arrayLength(SE_NAMES) * Audio::soundEffectsProfileCount; i++)
						{
							Audio::SoundInstance& sound = context.audio.debugSounds[i];

							ImGui::PushID(i);
							ImGui::TableNextRow(0, rowHeight);
							ImGui::TableSetColumnIndex(0);

							float ratio = sound.getCurrentFrame() /
							              static_cast<float>(sound.getLengthInFrames());
							if (!sound.isPlaying())
								ratio = 0.0f;

							ImGui::ProgressBar(ratio, { -1, 0 }, sound.name.c_str());

							float duration = sound.getDuration();
							int durationSecondsOnly = duration;
							float time = sound.isPlaying() ? sound.getCurrentTime() : 0.0f;
							int timeSecondsOnly = time;
							ImGui::TableSetColumnIndex(1);
							ImGui::Text("%02d:%02d/%02d:%02d", timeSecondsOnly,
							            static_cast<int>((time - timeSecondsOnly) * 100),
							            durationSecondsOnly,
							            static_cast<int>((duration - durationSecondsOnly) * 100));

							ImGui::TableSetColumnIndex(2);
							if (UI::transparentButton(ICON_FA_PLAY, UI::btnSmall))
							{
								sound.seek(0);
								sound.play();
							}

							ImGui::SameLine();
							ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
							ImGui::SameLine();
							if (UI::transparentButton(ICON_FA_STOP, UI::btnSmall))
								sound.stop();

							ImGui::PopID();
						}

						ImGui::EndTable();
					}
				}

				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Timeline", treeNodeFlags))
			{
				timeline.debug(context);
				ImGui::TreePop();
			}
		}

		ImGui::End();
	}

	void SettingsWindow::updateKeyConfig(MultiInputBinding* bindings[], int count)
	{
		ImVec2 size = ImVec2(-1, ImGui::GetContentRegionAvail().y * 0.7);
		const ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersOuter |
		                                   ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY |
		                                   ImGuiTableFlags_RowBg;

		const ImGuiSelectableFlags selectionFlags =
		    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		int rowHeight = ImGui::GetFrameHeight() + 5;

		if (ImGui::BeginTable("##commands_table", 2, tableFlags, size))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn(getString("action"));
			ImGui::TableSetupColumn(getString("keys"));
			ImGui::TableHeadersRow();

			for (int i = 0; i < count; ++i)
			{
				ImGui::TableNextRow(0, rowHeight);

				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(i);
				if (ImGui::Selectable(getString(bindings[i]->name), i == selectedBindingIndex,
				                      selectionFlags))
					selectedBindingIndex = i;

				ImGui::PopID();
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", ToFullShortcutsString(*bindings[i]).c_str());
			}
			ImGui::EndTable();
		}
		ImGui::Separator();

		if (selectedBindingIndex > -1 && selectedBindingIndex < count)
		{
			int deleteBinding = -1;
			int moveIndex = -1;
			int moveDirection = 0;
			const bool canAdd = bindings[selectedBindingIndex]->count < 4;

			const float btnHeight = ImGui::GetFrameHeight();

			UI::beginPropertyColumns();
			ImGui::Text("%s", getString(bindings[selectedBindingIndex]->name));
			ImGui::NextColumn();

			if (!canAdd)
				UI::beginNextItemDisabled();

			if (ImGui::Button(getString("add"), { -1, btnHeight }))
				bindings[selectedBindingIndex]->addBinding(InputBinding{});

			if (!canAdd)
				UI::endNextItemDisabled();

			UI::endPropertyColumns();

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));
			ImGui::BeginChild("##binding_keys_edit_window", ImVec2(-1, -1), true);

			float btnWidth = (UI::btnSmall.x * 2) + 100 + (ImGui::GetStyle().ItemSpacing.x * 3);
			for (int b = 0; b < bindings[selectedBindingIndex]->count; ++b)
			{
				const bool canMoveDown = !(bindings[selectedBindingIndex]->count <= b + 1);
				const bool canMoveUp = !(b < 1);
				ImGui::PushID(b);

				std::string buttonText =
				    ToShortcutString(bindings[selectedBindingIndex]->bindings[b]);
				;
				if (!buttonText.size())
					buttonText = getString("none");

				if (listeningForInput && editBindingIndex == b)
					buttonText = getString("cmd_key_listen");

				if (ImGui::Button(buttonText.c_str(),
				                  ImVec2(ImGui::GetContentRegionAvail().x - btnWidth, btnHeight)))
				{
					listeningForInput = true;
					inputTimer.reset();
					editBindingIndex = b;
				}

				ImGui::SameLine();
				if (!canMoveUp)
					UI::beginNextItemDisabled();
				if (ImGui::Button(ICON_FA_CARET_UP, { btnHeight, btnHeight }))
				{
					moveIndex = b;
					moveDirection = -1;
				}
				if (!canMoveUp)
					UI::endNextItemDisabled();

				ImGui::SameLine();
				if (!canMoveDown)
					UI::beginNextItemDisabled();
				if (ImGui::Button(ICON_FA_CARET_DOWN, { btnHeight, btnHeight }))
				{
					moveIndex = b;
					moveDirection = 1;
				}
				if (!canMoveDown)
					UI::endNextItemDisabled();

				ImGui::SameLine();
				if (ImGui::Button(getString("remove"), { -1, btnHeight }))
					deleteBinding = b;

				ImGui::PopID();
			}
			ImGui::EndChild();
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();

			if (moveIndex > -1)
			{
				listeningForInput = false;
				if (moveDirection == -1)
					bindings[selectedBindingIndex]->moveUp(moveIndex);
				if (moveDirection == 1)
					bindings[selectedBindingIndex]->moveDown(moveIndex);
			}

			if (deleteBinding > -1)
			{
				listeningForInput = false;
				bindings[selectedBindingIndex]->removeAt(deleteBinding);
			}
		}

		if (listeningForInput)
		{
			if (inputTimer.elapsed() >= inputTimeoutSeconds)
			{
				listeningForInput = false;
				editBindingIndex = -1;
			}
			else
			{
				for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_MouseLeft; ++key)
				{
					bool isCtrl = key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
					              key == ImGuiMod_Ctrl;
					bool isShift = key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
					               key == ImGuiMod_Shift;
					bool isAlt = key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
					             key == ImGuiMod_Alt;
					bool isSuper = key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper ||
					               key == ImGuiMod_Super;

					// execute if a non-modifier key is tapped
					if (ImGui::IsKeyPressed((ImGuiKey)key) && !isCtrl && !isShift && !isAlt &&
					    !isSuper)
					{
						bindings[selectedBindingIndex]->bindings[editBindingIndex] =
						    InputBinding((ImGuiKey)key, (int)ImGui::GetIO().KeyMods);
						listeningForInput = false;
						editBindingIndex = -1;
					}


				}
			}
		}
	}

	DialogResult SettingsWindow::update()
	{
		if (open)
		{
			ImGui::OpenPopup(MODAL_TITLE("settings"));
			open = false;
		}

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always,
		                        ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(750, 600), ImGuiCond_Always);
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));

		if (ImGui::BeginPopupModal(MODAL_TITLE("settings"), NULL, ImGuiWindowFlags_NoResize))
		{
			ImVec2 padding = ImGui::GetStyle().WindowPadding;
			ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
			ImVec2 confirmBtnPos =
			    ImGui::GetWindowSize() + ImVec2(-100, -UI::btnNormal.y) - padding;
			ImGui::BeginChild("##settings_panel", ImGui::GetContentRegionAvail() -
			                                          ImVec2(0, UI::btnNormal.y + padding.y));

			if (ImGui::BeginTabBar("##settings_tabs"))
			{
				if (ImGui::BeginTabItem(IMGUI_TITLE("", "general")))
				{
					if (ImGui::CollapsingHeader(getString("language"),
					                            ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::propertyLabel(getString("language"));

						std::string curr = getString("auto");
						auto langIt = Localization::languages.find(config.language);
						if (langIt != Localization::languages.end())
							curr = langIt->second->getString("language_name");

						if (ImGui::BeginCombo("##language", curr.c_str()))
						{
							if (ImGui::Selectable(getString("auto"), config.language == "auto"))
								config.language = "auto";

							for (const auto& [code, language] : Localization::languages)
							{
								const bool selected = curr == code;
								std::string str = language->getString("language_name");

								if (ImGui::Selectable(str.c_str(), selected))
									config.language = code;
							}

							ImGui::EndCombo();
						}

						UI::endPropertyColumns();
					}

					// if (ImGui::CollapsingHeader(getString("file"),
					// ImGuiTreeNodeFlags_DefaultOpen))
					//{
					//	UI::beginPropertyColumns();
					//	UI::addCheckboxProperty(getString("minify"), config.minifyOutput);
					//	UI::endPropertyColumns();
					// }

					if (ImGui::CollapsingHeader(getString("auto_save"),
					                            ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addCheckboxProperty(getString("auto_save_enable"),
						                        config.autoSaveEnabled);
						UI::addIntProperty(getString("auto_save_interval"),
						                   config.autoSaveInterval);
						UI::addIntProperty(getString("auto_save_count"), config.autoSaveMaxCount);
						UI::endPropertyColumns();
					}

					if (ImGui::CollapsingHeader(getString("theme"), ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addSelectProperty(getString("base_theme"), config.baseTheme, baseThemes,
						                      (int)BaseTheme::BASE_THEME_MAX);
						UI::endPropertyColumns();

						ImGui::TextWrapped("%s", getString("accent_color_help"));
						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
						                    ImVec2(ImGui::GetStyle().ItemSpacing.x + 3, 15));
						ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);

						for (int i = 0; i < UI::accentColors.size(); ++i)
						{
							bool apply = false;
							std::string id = i == config.accentColor ? ICON_FA_CHECK
							                 : i == 0                ? "C"
							                                         : "##" + std::to_string(i);
							ImGui::PushStyleColor(ImGuiCol_Button, UI::accentColors[i]);
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::accentColors[i]);
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::accentColors[i]);

							apply = ImGui::Button(id.c_str(), UI::btnNormal);

							ImGui::PopStyleColor(3);

							if ((i < UI::accentColors.size() - 1) &&
							    ImGui::GetCursorPosX() <
							        ImGui::GetWindowSize().x - UI::btnNormal.x - 50.0f)
								ImGui::SameLine();

							if (apply)
								config.accentColor = i;
						}
						ImGui::PopStyleVar(2);

						ImVec4& customColor = UI::accentColors[0];
						float col[]{ customColor.x, customColor.y, customColor.z };
						static ColorDisplay displayMode = ColorDisplay::HEX;

						ImGui::Separator();
						ImGui::Text("%s", getString("select_accent_color"));
						UI::beginPropertyColumns();
						UI::propertyLabel(getString("display_mode"));
						if (ImGui::BeginCombo("##color_display_mode",
						                      colorDisplayStr[(int)displayMode]))
						{
							for (int i = 0; i < 3; ++i)
							{
								const bool selected = (int)displayMode == i;
								if (ImGui::Selectable(colorDisplayStr[i], selected))
									displayMode = (ColorDisplay)i;
							}

							ImGui::EndCombo();
						}
						ImGui::NextColumn();
						UI::propertyLabel(getString("custom_color"));

						ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
						                    ImVec2(ImGui::GetStyle().ItemSpacing.x + 3, 15));
						ImGuiColorEditFlags flags = 1 << (20 + (int)displayMode);
						if (ImGui::ColorEdit3("##custom_accent_color", col, flags))
							customColor.x = col[0];
						customColor.y = col[1];
						customColor.z = col[2];

						UI::endPropertyColumns();
						ImGui::PopStyleVar();
					}

					if (ImGui::CollapsingHeader(getString("video"), ImGuiTreeNodeFlags_DefaultOpen))
					{
						bool vsync = Application::windowState.vsync;
						UI::beginPropertyColumns();
						UI::addCheckboxProperty(getString("vsync"), Application::windowState.vsync);
						UI::endPropertyColumns();

						if (vsync != Application::windowState.vsync)
							glfwSwapInterval(Application::windowState.vsync ? 1 : 0);
					}

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem(IMGUI_TITLE("", "timeline")))
				{
					if (ImGui::CollapsingHeader(getString("timeline"),
					                            ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addCheckboxProperty(getString("match_notes_size_to_timeline"),
						                        config.matchNotesSizeToTimeline);
						UI::addIntProperty(getString("lane_width"), config.timelineWidth, "%dpx",
						                   MIN_LANE_WIDTH, MAX_LANE_WIDTH);

						if (config.matchNotesSizeToTimeline)
							UI::beginNextItemDisabled();
						UI::addIntProperty(getString("notes_height"), config.notesHeight, "%dpx",
						                   MIN_NOTES_HEIGHT, MAX_NOTES_HEIGHT);
						if (config.matchNotesSizeToTimeline)
							UI::endNextItemDisabled();
						ImGui::Separator();

						UI::addCheckboxProperty(getString("draw_waveform"), config.drawWaveform);
						UI::addCheckboxProperty(getString("draw_hispeed_automation"), config.drawHiSpeedAutomation);
						UI::addSliderProperty(getString("hispeed_graph_limit"), config.hiSpeedGraphLimit, 1.0f, 10.0f, "%.1fx");
						UI::addPercentSliderProperty(getString("hispeed_graph_bg_opacity"), config.hiSpeedGraphBgOpacity);
						UI::addCheckboxProperty(getString("return_to_last_tick"), config.returnToLastSelectedTickOnPause);
						UI::addCheckboxProperty(getString("cursor_auto_scroll"), config.followCursorInPlayback);
						UI::addPercentSliderProperty(getString("cursor_auto_scroll_amount"), config.cursorPositionThreshold);
						UI::endPropertyColumns();
					}

					if (ImGui::CollapsingHeader(getString("scrolling"),
					                            ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addFloatProperty(getString("scroll_speed_normal"),
						                     config.scrollSpeedNormal, "%.1fx");
						UI::addFloatProperty(getString("scroll_speed_shift"),
						                     config.scrollSpeedShift, "%.1fx");
						config.scrollSpeedNormal = std::max(0.1f, config.scrollSpeedNormal);
						config.scrollSpeedShift = std::max(0.1f, config.scrollSpeedShift);
						ImGui::Separator();

						UI::addCheckboxProperty(getString("use_smooth_scroll"),
						                        config.useSmoothScrolling);
						UI::addSliderProperty(getString("smooth_scroll_time"),
						                      config.smoothScrollingTime, 10.0f, 150.0f, "%.2fms");
						UI::endPropertyColumns();
					}

					if (ImGui::CollapsingHeader(getString("audio"), ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addSelectProperty(getString("notes_se"), config.seProfileIndex,
						                      Audio::soundEffectsProfileNames,
						                      Audio::soundEffectsProfileCount);
						UI::endPropertyColumns();
					}

					if (ImGui::CollapsingHeader(getString("background"),
					                            ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addCheckboxProperty(getString("draw_background"),
						                        config.drawBackground);

						std::string backgroundFile = config.backgroundImage;
						int result =
						    UI::addFileProperty(getString("background_image"), backgroundFile);
						if (result == 1)
						{
							config.backgroundImage = backgroundFile;
							isBackgroundChangePending = true;
						}
						else if (result == 2)
						{
							IO::FileDialog fileDialog{};
							fileDialog.title = "Open Image File";
							fileDialog.filters = { IO::imageFilter, IO::allFilter };
							fileDialog.parentWindowHandle = Application::windowState.windowHandle;

							if (fileDialog.openFile() == IO::FileDialogResult::OK)
							{
								config.backgroundImage = fileDialog.outputFilename;
								isBackgroundChangePending = true;
							}
						}

						UI::addPercentSliderProperty(getString("background_brightnes"),
						                             config.backgroundBrightness);
						ImGui::Separator();

						UI::addPercentSliderProperty(getString("lanes_opacity"),
						                             config.laneOpacity);
						UI::endPropertyColumns();
					}

					if (ImGui::CollapsingHeader(getString("advanced"),
					                            ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addCheckboxProperty(getString("show_tick_in_properties"),
						                        config.showTickInProperties);
						UI::endPropertyColumns();
					}

					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem(IMGUI_TITLE("", "preview")))
				{
					if (ImGui::CollapsingHeader(getString("general"), ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addSliderProperty(getString("notes_speed"), config.pvNoteSpeed, 1, 12, "%.2f");
						UI::addCheckboxProperty(getString("mirror_score"), config.pvMirrorScore);
						UI::addCheckboxProperty(getString("preview_draw_toolbar"), config.pvDrawToolbar);
						UI::endPropertyColumns();
					}

					if (ImGui::CollapsingHeader(getString("background"), ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addCheckboxProperty(getString("draw_background"), config.drawBackground);

						std::string backgroundFile = config.backgroundImage;
						int result = UI::addFileProperty(getString("background_image"), backgroundFile);
						if (result == 1)
						{
							config.backgroundImage = backgroundFile;
							isBackgroundChangePending = true;
						}
						else if (result == 2)
						{
							IO::FileDialog fileDialog{};
							fileDialog.title = "Open Image File";
							fileDialog.filters = { IO::imageFilter, IO::allFilter };
							fileDialog.parentWindowHandle = Application::windowState.windowHandle;

							if (fileDialog.openFile() == IO::FileDialogResult::OK)
							{
								config.backgroundImage = fileDialog.outputFilename;
								isBackgroundChangePending = true;
							}
						}

						UI::addPercentSliderProperty(getString("background_brightnes"), config.pvBackgroundBrightness);
						ImGui::Separator();

						UI::addPercentSliderProperty(getString("stage_opacity"), config.pvStageOpacity);
						UI::addPercentSliderProperty(getString("stage_cover"), config.pvStageCover);
						UI::endPropertyColumns();
					}

					if (ImGui::CollapsingHeader(getString("audio"), ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addSelectProperty(getString("notes_se"), config.seProfileIndex, Audio::soundEffectsProfileNames, Audio::soundEffectsProfileCount);
						UI::endPropertyColumns();
					}
					
					if (ImGui::CollapsingHeader(getString("visuals"), ImGuiTreeNodeFlags_DefaultOpen))
					{
						UI::beginPropertyColumns();
						UI::addCheckboxProperty(getString("flicks_animation"), config.pvFlickAnimation);
						UI::addCheckboxProperty(getString("holds_animation"), config.pvHoldAnimation);
						UI::addCheckboxProperty(getString("simultaneous_lines"), config.pvSimultaneousLine);
						ImGui::Separator();

						float hold_alpha = config.pvHoldAlpha * 100.f;
						UI::addSliderProperty(getString("holds_alpha"), hold_alpha, 10, 100, "%.0f%%");
						config.pvHoldAlpha = hold_alpha / 100.f;

						float guide_alpha = config.pvGuideAlpha * 100.f;
						UI::addSliderProperty(getString("guides_alpha"), guide_alpha, 10, 100, "%.0f%%");
						config.pvGuideAlpha = guide_alpha / 100.f;
						
						UI::endPropertyColumns();
					}
					
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem(IMGUI_TITLE("", "key_config")))
				{
					updateKeyConfig(bindings, sizeof(bindings) / sizeof(MultiInputBinding*));
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();

			ImGui::EndChild();
			ImGui::SetCursorPos(confirmBtnPos);
			if (ImGui::Button("OK", ImVec2(100, UI::btnNormal.y - 5)))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();
		return DialogResult::None;
	}

	void LayersWindow::update(ScoreContext& context)
	{
		static int activeSoloIndex = -1;
		static std::vector<bool> preSoloHiddenStates;
		static bool focusRenameInput = false;

		static size_t lastLayerCount = context.score.layers.size();
		if (lastLayerCount != context.score.layers.size()) {
			activeSoloIndex = -1;
			preSoloHiddenStates.clear();
			lastLayerCount = context.score.layers.size();
		}

		bool doMergeLayer = false;
		bool doDeleteLayer = false;

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_LAYER_GROUP, "layers")))
		{
			int toggleHideIndex = -1;
			int soloIndex = -1;
			int dragDropFrom = -1;
			int dragDropTo = -1;
			int dragDropType = -1; 

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
			float layersButtonHeight = ImGui::GetFrameHeight();

			// =====================================================================
			// 上部アイコンバー
			// =====================================================================
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

			bool showAllLayers = context.showAllLayers;
			if (showAllLayers) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
			if (UI::transparentButton(ICON_FA_EYE, ImVec2(layersButtonHeight, layersButtonHeight), false))
				context.showAllLayers = !context.showAllLayers;
			if (showAllLayers) ImGui::PopStyleColor();
			UI::tooltip(getString("show_all_layers"));

			float rightWidth = (layersButtonHeight * 4.0f) + (4.0f * 3.0f);
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - rightWidth);

			if (UI::transparentButton(ICON_FA_PLUS, ImVec2(layersButtonHeight, layersButtonHeight), false))
			{
				Layer newLayer;
				newLayer.name = getString("new_layer"); // 日本語化
				newLayer.isFolder = false;
				context.score.layers.push_back(newLayer);
				
				id_t id = getNextHiSpeedID();
				context.score.hiSpeedChanges[id] = { id, 0, 1, static_cast<int>(context.score.layers.size()) - 1 };
				
				renameIndex = context.score.layers.size() - 1;
				layerName = newLayer.name;
				context.selectedLayer = renameIndex;
				focusRenameInput = true;
			}
			UI::tooltip(getString("create_layer"));
			ImGui::SameLine();

			if (UI::transparentButton(ICON_FA_FOLDER_PLUS, ImVec2(layersButtonHeight, layersButtonHeight), false))
			{
				Layer newLayer;
				newLayer.name = getString("new_folder"); // 日本語化
				newLayer.isFolder = true;
				context.score.layers.push_back(newLayer);
				
				renameIndex = context.score.layers.size() - 1;
				layerName = newLayer.name;
				context.selectedLayer = renameIndex;
				focusRenameInput = true;
			}
			UI::tooltip(getString("create_folder")); // 日本語化
			ImGui::SameLine();

			bool canMerge = context.selectedLayer >= 0 && context.selectedLayer < context.score.layers.size() - 1 && !context.score.layers[context.selectedLayer].isFolder;
			if (!canMerge) ImGui::BeginDisabled();
			if (UI::transparentButton(ICON_FA_LEVEL_DOWN_ALT, ImVec2(layersButtonHeight, layersButtonHeight), false))
			{
				ImGui::OpenPopup(getString("layer_merge_confirm")); // 日本語化
			}
			if (!canMerge) ImGui::EndDisabled();
			UI::tooltip(getString("layer_merge"));
			ImGui::SameLine();

			bool canDelete = context.selectedLayer >= 0 && context.score.layers.size() > 1;
			if (!canDelete) ImGui::BeginDisabled();
			if (UI::transparentButton(ICON_FA_TRASH, ImVec2(layersButtonHeight, layersButtonHeight), false))
			{
				ImGui::OpenPopup(getString("layer_delete_confirm")); // 日本語化
			}
			if (!canDelete) ImGui::EndDisabled();
			UI::tooltip(getString("delete"));

			ImGui::PopStyleVar();
			ImGui::Separator();
			// =====================================================================

			float windowHeight = ImGui::GetContentRegionAvail().y - ImGui::GetStyle().WindowPadding.y;

			if (ImGui::BeginChild("layers_child_window", ImVec2(-1, windowHeight), true))
			{
				bool hideChildren = false;

				for (int index = 0; index < context.score.layers.size(); ++index)
				{
					const auto& layer = context.score.layers[index];

					if (layer.isFolder) hideChildren = layer.isCollapsed;
					else if (!layer.inFolder) hideChildren = false;

					if (layer.inFolder && hideChildren) continue;

					ImGui::PushID(index);
					ImVec2 startPos = ImGui::GetCursorScreenPos();
					float width = ImGui::GetContentRegionAvail().x;
					bool isSelected = (index == context.selectedLayer);

					if (isSelected)
					{
						ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
						ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
					}

					if (ImGui::Selectable((std::string("##row_") + std::to_string(index)).c_str(), isSelected, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(width, layersButtonHeight)))
					{
						context.selectedLayer = index;
					}

					if (isSelected) ImGui::PopStyleColor(2);

					if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						renameIndex = index;
						layerName = layer.name;
						focusRenameInput = true;
					}

					int currentDropType = -1;
					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
					{
						ImGui::SetDragDropPayload("LAYER_REORDER", &index, sizeof(int));
						ImGui::Text("%s %s", layer.isFolder ? ICON_FA_FOLDER : ICON_FA_FILE, layer.name.c_str());
						ImGui::EndDragDropSource();
					}
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
						{
							if (payload->IsDataType("LAYER_REORDER"))
							{
								ImVec2 min = ImGui::GetItemRectMin();
								ImVec2 max = ImGui::GetItemRectMax();
								float itemHeight = max.y - min.y;
								float mouseY = ImGui::GetMousePos().y - min.y;

								if (layer.isFolder && mouseY > itemHeight * 0.25f && mouseY < itemHeight * 0.75f) currentDropType = 1;
								else if (mouseY < itemHeight * 0.5f) currentDropType = 0;
								else currentDropType = 2;

								ImU32 redColor = IM_COL32(255, 80, 80, 255);
								if (currentDropType == 0) ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, min.y), ImVec2(max.x, min.y), redColor, 2.0f);
								else if (currentDropType == 2) ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y), ImVec2(max.x, max.y), redColor, 2.0f);
								else ImGui::GetWindowDrawList()->AddRect(min, max, redColor, 0.0f, 0, 2.0f);
							}
						}
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LAYER_REORDER", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
						{
							dragDropFrom = *(const int*)payload->Data;
							dragDropTo = index;
							dragDropType = currentDropType;
						}
						ImGui::EndDragDropTarget();
					}

					ImGui::SetCursorScreenPos(ImVec2(startPos.x + ImGui::GetStyle().FramePadding.x, startPos.y));
					
					float indent = layer.inFolder ? 24.0f : 0.0f;
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);

					if (layer.inFolder)
					{
						bool isLastChild = true;
						for (int j = index + 1; j < context.score.layers.size(); ++j)
						{
							if (context.score.layers[j].inFolder) { isLastChild = false; break; }
							if (context.score.layers[j].isFolder) break;
							break;
						}

						float lineX = startPos.x + 14.0f;
						float spaceY = ImGui::GetStyle().ItemSpacing.y;
						float topY = startPos.y - spaceY * 0.5f;
						float midY = startPos.y + (layersButtonHeight * 0.5f);
						float bottomY = startPos.y + layersButtonHeight + spaceY * 0.5f;

						ImU32 lineColor = IM_COL32(120, 120, 120, 255);
						ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, topY), ImVec2(lineX, isLastChild ? midY : bottomY), lineColor, 1.0f);
						ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, midY), ImVec2(lineX + 10.0f, midY), lineColor, 1.0f);
					}

					ImVec2 caretSize = ImVec2(ImGui::CalcTextSize(ICON_FA_CARET_DOWN).x + 4.0f, layersButtonHeight);
					if (layer.isFolder)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
						if (ImGui::Button(layer.isCollapsed ? ICON_FA_CARET_RIGHT : ICON_FA_CARET_DOWN, caretSize))
							context.score.layers[index].isCollapsed = !layer.isCollapsed;
						ImGui::PopStyleColor();
						ImGui::SameLine();
						ImGui::AlignTextToFramePadding();
						ImGui::Text("%s", ICON_FA_FOLDER);
						ImGui::SameLine();
					}
					else
					{
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() + caretSize.x + ImGui::GetStyle().ItemSpacing.x);
					}

					float rightPanelWidth = UI::btnSmall.x * 2 + ImGui::GetStyle().ItemSpacing.x * 1 + 10.0f;

					if (renameIndex == index)
					{
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - rightPanelWidth - 10.0f);
						
						if (focusRenameInput)
						{
							ImGui::SetKeyboardFocusHere();
							focusRenameInput = false;
						}
						
						if (ImGui::InputText((std::string("##rename_") + std::to_string(index)).c_str(), &layerName, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
						{
							Score prev = context.score;
							context.score.layers[index].name = layerName;
							renameIndex = -1;
							context.pushHistory("Rename Layer", prev, context.score);
						}
						else if (ImGui::IsItemDeactivated())
						{
							if (ImGui::IsKeyPressed(ImGuiKey_Escape))
							{
								renameIndex = -1;
							}
							else
							{
								Score prev = context.score;
								context.score.layers[index].name = layerName;
								renameIndex = -1;
								context.pushHistory("Rename Layer", prev, context.score);
							}
						}
					}
					else
					{
						ImGui::AlignTextToFramePadding();
						ImGui::Text("%s", layer.name.c_str());
					}

					float sameLineX = width - rightPanelWidth;
					if (sameLineX > ImGui::GetStyle().FramePadding.x)
						ImGui::SameLine(sameLineX);
					else
						ImGui::SameLine();

					if (UI::transparentButton(layer.hidden ? ICON_FA_EYE_SLASH : ICON_FA_EYE, ImVec2(UI::btnSmall.x, layersButtonHeight), false))
						toggleHideIndex = index;

					ImGui::SameLine();
					if (!layer.isFolder) {
						if (activeSoloIndex == index) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_PlotLinesHovered));
						if (ImGui::Button("S", ImVec2(UI::btnSmall.x, layersButtonHeight))) soloIndex = index;
						if (activeSoloIndex == index) ImGui::PopStyleColor();
					} else {
						ImGui::Dummy(ImVec2(UI::btnSmall.x, layersButtonHeight));
					}

					ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + layersButtonHeight + ImGui::GetStyle().ItemSpacing.y));
					ImGui::PopID();
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();

			if (dragDropFrom != -1 && dragDropTo != -1)
			{
				Score prev = context.score;
				
				int srcStart = dragDropFrom;
				int srcCount = 1;
				if (context.score.layers[srcStart].isFolder) {
					while (srcStart + srcCount < context.score.layers.size() && context.score.layers[srcStart + srcCount].inFolder) {
						srcCount++;
					}
				}
				
				int insertPos = dragDropTo;
				bool newInFolder = false;

				if (dragDropType == 1) {
					insertPos = dragDropTo + 1;
					newInFolder = true;
				} else if (dragDropType == 0) {
					insertPos = dragDropTo;
					newInFolder = context.score.layers[dragDropTo].inFolder;
				} else {
					insertPos = dragDropTo + 1;
					newInFolder = context.score.layers[dragDropTo].inFolder;
				}

				if (!(insertPos >= srcStart && insertPos <= srcStart + srcCount))
				{
					if (!context.score.layers[srcStart].isFolder) {
						context.score.layers[srcStart].inFolder = newInFolder;
					}

					std::vector<int> oldIndices(context.score.layers.size());
					for (int i = 0; i < oldIndices.size(); ++i) oldIndices[i] = i;
					
					std::vector<int> oldBlock(oldIndices.begin() + srcStart, oldIndices.begin() + srcStart + srcCount);
					oldIndices.erase(oldIndices.begin() + srcStart, oldIndices.begin() + srcStart + srcCount);
					if (insertPos > srcStart) insertPos -= srcCount;
					oldIndices.insert(oldIndices.begin() + insertPos, oldBlock.begin(), oldBlock.end());
					
					std::unordered_map<int, int> oldToNew;
					for (int newIdx = 0; newIdx < oldIndices.size(); ++newIdx) oldToNew[oldIndices[newIdx]] = newIdx;

					std::vector<Layer> newLayers(context.score.layers.size());
					for (int i = 0; i < oldIndices.size(); ++i) {
						newLayers[i] = context.score.layers[oldIndices[i]];
					}
					context.score.layers = newLayers;

					for (auto& [_, note] : context.score.notes) note.layer = oldToNew[note.layer];
					for (auto& [_, hiSpeed] : context.score.hiSpeedChanges) hiSpeed.layer = oldToNew[hiSpeed.layer];
					context.selectedLayer = oldToNew[context.selectedLayer];

					context.pushHistory("Reorder Layers/Folders", prev, context.score);
				}
			}

			if (toggleHideIndex != -1)
			{
				Score prev = context.score;
				auto& layer = context.score.layers[toggleHideIndex];
				layer.hidden = !layer.hidden;
				
				if (layer.isFolder) {
					for (int i = toggleHideIndex + 1; i < context.score.layers.size(); ++i) {
						if (!context.score.layers[i].inFolder) break;
						context.score.layers[i].hidden = layer.hidden;
					}
				}
				context.pushHistory("Toggle Hide Layer", prev, context.score);
			}

			if (soloIndex != -1)
			{
				Score prev = context.score;
				if (activeSoloIndex == soloIndex)
				{
					for (int i = 0; i < context.score.layers.size() && i < preSoloHiddenStates.size(); ++i)
					{
						context.score.layers[i].hidden = preSoloHiddenStates[i];
					}
					activeSoloIndex = -1;
				}
				else
				{
					if (activeSoloIndex == -1)
					{
						preSoloHiddenStates.clear();
						for (const auto& l : context.score.layers) preSoloHiddenStates.push_back(l.hidden);
					}
					for (int i = 0; i < context.score.layers.size(); ++i)
					{
						context.score.layers[i].hidden = (i != soloIndex);
					}
					activeSoloIndex = soloIndex;
				}
				context.pushHistory("Toggle Solo Layer", prev, context.score);
			}

			// =====================================================================
			// 日本語化されたポップアップ
			// =====================================================================
			if (ImGui::BeginPopupModal(getString("layer_delete_confirm"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
			{
				ImGui::Text("%s", getString("layer_delete_msg1"));
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", getString("layer_delete_msg2"));
				ImGui::Separator();
				
				if (ImGui::Button(getString("yes"), ImVec2(120, 0))) {
					doDeleteLayer = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button(getString("cancel"), ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			if (ImGui::BeginPopupModal(getString("layer_merge_confirm"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
			{
				ImGui::Text("%s", getString("layer_merge_msg"));
				ImGui::Separator();
				
				if (ImGui::Button(getString("yes"), ImVec2(120, 0))) {
					doMergeLayer = true;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button(getString("cancel"), ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();

		if (doMergeLayer)
		{
			int mergePattern = context.selectedLayer;
			Score prev = context.score;
			context.score.layers.erase(context.score.layers.begin() + mergePattern);
			for (auto& [_, note] : context.score.notes)
			{
				if (note.layer > mergePattern) note.layer -= 1;
			}
			for (auto& [_, hiSpeed] : context.score.hiSpeedChanges)
			{
				if (hiSpeed.layer > mergePattern) hiSpeed.layer -= 1;
			}
			context.pushHistory("Merge Layer", prev, context.score);
		}

		if (doDeleteLayer)
		{
			int deleteIndex = context.selectedLayer;
			int delCount = 1;
			if (context.score.layers[deleteIndex].isFolder) {
				while (deleteIndex + delCount < context.score.layers.size() && context.score.layers[deleteIndex + delCount].inFolder) {
					delCount++;
				}
			}

			if (context.score.layers.size() - delCount >= 1)
			{
				Score prev = context.score;

				std::unordered_set<int> notesToDelete;
				for (const auto& [id, note] : context.score.notes) {
					if (note.layer >= deleteIndex && note.layer < deleteIndex + delCount) notesToDelete.insert(id);
				}

				for (auto id : notesToDelete) {
					auto notePos = context.score.notes.find(id);
					if (notePos == context.score.notes.end()) continue;

					Note& note = notePos->second;
					if (note.getType() != NoteType::Hold && note.getType() != NoteType::HoldEnd) {
						if (note.getType() == NoteType::HoldMid && context.score.holdNotes.count(note.parentID)) {
							std::vector<HoldStep>& steps = context.score.holdNotes.at(note.parentID).steps;
							auto stepIt = std::find_if(steps.cbegin(), steps.cend(), [id](const HoldStep& s) { return s.ID == id; });
							if (stepIt != steps.cend()) steps.erase(stepIt);
						}
						context.score.notes.erase(id);
					} else {
						const HoldNote& hold = context.score.holdNotes.at(note.getType() == NoteType::Hold ? note.ID : note.parentID);
						context.score.notes.erase(hold.start.ID);
						context.score.notes.erase(hold.end);
						for (const auto& step : hold.steps) context.score.notes.erase(step.ID);
						context.score.holdNotes.erase(hold.start.ID);
					}
				}

				std::unordered_set<int> hiSpeedsToDelete;
				for (const auto& [id, hs] : context.score.hiSpeedChanges) {
					if (hs.layer >= deleteIndex && hs.layer < deleteIndex + delCount) hiSpeedsToDelete.insert(id);
				}
				for (auto id : hiSpeedsToDelete) context.score.hiSpeedChanges.erase(id);

				for (auto it = context.selectedNotes.begin(); it != context.selectedNotes.end(); ) {
					if (!context.score.notes.count(*it)) it = context.selectedNotes.erase(it);
					else ++it;
				}
				for (auto it = context.selectedHiSpeedChanges.begin(); it != context.selectedHiSpeedChanges.end(); ) {
					if (!context.score.hiSpeedChanges.count(*it)) it = context.selectedHiSpeedChanges.erase(it);
					else ++it;
				}

				context.score.layers.erase(context.score.layers.begin() + deleteIndex, context.score.layers.begin() + deleteIndex + delCount);
				for (auto& [_, note] : context.score.notes) if (note.layer >= deleteIndex + delCount) note.layer -= delCount;
				for (auto& [_, hiSpeed] : context.score.hiSpeedChanges) if (hiSpeed.layer >= deleteIndex + delCount) hiSpeed.layer -= delCount;
				
				if (context.selectedLayer >= deleteIndex && context.selectedLayer < deleteIndex + delCount) context.selectedLayer = 0;
				else if (context.selectedLayer >= deleteIndex + delCount) context.selectedLayer -= delCount;

				context.pushHistory("Delete Layer/Folder", prev, context.score);
			}
		}
	}

	DialogResult LayersWindow::updateCreationDialog()
	{
		DialogResult result = DialogResult::None;

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always,
		                        ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Always);
		ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
		
		bool popupOpened = false;
		if (renameIndex == -2)
			popupOpened = ImGui::BeginPopupModal("Create Folder", NULL, ImGuiWindowFlags_NoResize);
		else if (renameIndex >= 0)
			popupOpened = ImGui::BeginPopupModal(MODAL_TITLE("layer_rename"), NULL, ImGuiWindowFlags_NoResize);
		else
			popupOpened = ImGui::BeginPopupModal(MODAL_TITLE("create_layer"), NULL, ImGuiWindowFlags_NoResize);

		if (popupOpened)
		{
			ImVec2 padding = ImGui::GetStyle().WindowPadding;
			ImVec2 spacing = ImGui::GetStyle().ItemSpacing;

			float xPos = padding.x;
			float yPos = ImGui::GetWindowSize().y - UI::btnSmall.y - 2.0f - (padding.y * 2);

			if (renameIndex == -2)
				ImGui::Text("Folder Name");
			else
				ImGui::Text("%s", getString("layer_name")); // 余計な .c_str() を削除

			ImGui::SetNextItemWidth(-1);
			ImGui::InputText("##layer_name", &layerName);

			ImVec2 btnSz{ (ImGui::GetContentRegionAvail().x - spacing.x - (padding.x * 0.5f)) /
			                  2.0f,
			              ImGui::GetFrameHeight() };
			ImGui::SetCursorPos(ImVec2(xPos, yPos));
			
			if (ImGui::Button(getString("cancel"), btnSz)) // 余計な .c_str() を削除
			{
				result = DialogResult::Cancel;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, !layerName.size());
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1 - (0.5f * !layerName.size()));
			
			if (ImGui::Button(getString("confirm"), btnSz)) // 余計な .c_str() を削除
			{
				result = DialogResult::Ok;
				ImGui::CloseCurrentPopup();
			}

			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
			ImGui::EndPopup();
		}

		return result;
	}

	void WaypointsWindow::update(ScoreContext& context)
	{
		static int renameIndex = -1;
		static std::string waypointName = "";
		static bool focusRenameInput = false;
		static int selectedWaypointIndex = -1;
		static bool descendingOrder = true;

		auto getContrastColor = [](const ImVec4& bg) -> ImVec4 {
			float luminance = bg.x * 0.299f + bg.y * 0.587f + bg.z * 0.114f;
			return luminance > 0.5f ? ImVec4(0.1f, 0.1f, 0.1f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		};

		ImVec4 activeBgColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
		ImVec4 activeTextColor = getContrastColor(activeBgColor);

		if (ImGui::Begin(IMGUI_TITLE(ICON_FA_LOCATION_ARROW, "waypoints")))
		{
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
			float waypointButtonHeight = ImGui::GetFrameHeight();

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

			if (UI::transparentButton(descendingOrder ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN, ImVec2(waypointButtonHeight, waypointButtonHeight), false))
			{
				descendingOrder = !descendingOrder;
			}
			UI::tooltip(descendingOrder ? getString("sort_asc") : getString("sort_desc")); // 日本語化

			float rightWpWidth = (waypointButtonHeight * 2.0f) + (4.0f * 1.0f);
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - rightWpWidth);

			if (UI::transparentButton(ICON_FA_PLUS, ImVec2(waypointButtonHeight, waypointButtonHeight), false))
			{
				Waypoint newWp{ getString("new_waypoint"), context.currentTick }; // 日本語化
				context.score.waypoints.push_back(newWp);
				
				std::sort(context.score.waypoints.begin(), context.score.waypoints.end(),
				          [](const Waypoint& a, const Waypoint& b) { return a.tick < b.tick; });

				auto it = std::find_if(context.score.waypoints.begin(), context.score.waypoints.end(),
					[&](const Waypoint& w) { return w.tick == newWp.tick && w.name == newWp.name; });
				
				if (it != context.score.waypoints.end()) {
					renameIndex = std::distance(context.score.waypoints.begin(), it);
					waypointName = newWp.name;
					selectedWaypointIndex = renameIndex;
					focusRenameInput = true;
				}
			}
			UI::tooltip(getString("create_waypoint"));
			ImGui::SameLine();

			bool canDelete = selectedWaypointIndex >= 0 && selectedWaypointIndex < context.score.waypoints.size();
			if (!canDelete) ImGui::BeginDisabled();
			if (UI::transparentButton(ICON_FA_TRASH, ImVec2(waypointButtonHeight, waypointButtonHeight), false))
			{
				context.score.waypoints.erase(context.score.waypoints.begin() + selectedWaypointIndex);
				if (selectedWaypointIndex >= context.score.waypoints.size()) selectedWaypointIndex--; 
				renameIndex = -1;
			}
			if (!canDelete) ImGui::EndDisabled();
			UI::tooltip(getString("delete"));

			ImGui::PopStyleVar();
			ImGui::Separator();

			float windowHeight = ImGui::GetContentRegionAvail().y - ImGui::GetStyle().WindowPadding.y;

			if (ImGui::BeginChild("waypoints_child_window", ImVec2(-1, windowHeight), true))
			{
				ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
				if (ImGui::BeginTable("waypoints_table", 2, tableFlags))
				{
					ImGui::TableSetupColumn(getString("name"), ImGuiTableColumnFlags_WidthStretch); // 日本語化
					ImGui::TableSetupColumn(getString("time"), ImGuiTableColumnFlags_WidthFixed, 140.0f); // 日本語化
					
					for (int i = 0; i < context.score.waypoints.size(); ++i)
					{
						int index = descendingOrder ? (context.score.waypoints.size() - 1 - i) : i;
						auto& waypoint = context.score.waypoints[index];
						
						ImGui::PushID(index);
						ImGui::TableNextRow(0, waypointButtonHeight);
						
						ImGui::TableSetColumnIndex(0); 

						ImVec2 startPos = ImGui::GetCursorScreenPos();
						bool isSelected = (index == selectedWaypointIndex);

						if (isSelected)
						{
							ImGui::PushStyleColor(ImGuiCol_Header, activeBgColor);
							ImGui::PushStyleColor(ImGuiCol_HeaderHovered, activeBgColor);
							ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor); 
						}

						if (ImGui::Selectable((std::string("##wp_row_") + std::to_string(index)).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, waypointButtonHeight)))
						{
							selectedWaypointIndex = index;
							context.currentTick = waypoint.tick;
							scrollTimeline(context, waypoint.tick);
						}

						if (isSelected) ImGui::PopStyleColor(3);

						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							renameIndex = index;
							waypointName = waypoint.name;
							focusRenameInput = true;
							selectedWaypointIndex = index;
						}

						ImGui::SetCursorScreenPos(ImVec2(startPos.x + ImGui::GetStyle().FramePadding.x, startPos.y));

						if (renameIndex == index)
						{
							ImGui::SetNextItemWidth(-1);
							if (focusRenameInput)
							{
								ImGui::SetKeyboardFocusHere();
								focusRenameInput = false;
							}
							
							if (ImGui::InputText((std::string("##wprename_") + std::to_string(index)).c_str(), &waypointName, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
							{
								context.score.waypoints[index].name = waypointName;
								renameIndex = -1;
							}
							else if (ImGui::IsItemDeactivated())
							{
								if (ImGui::IsKeyPressed(ImGuiKey_Escape)) renameIndex = -1;
								else { context.score.waypoints[index].name = waypointName; renameIndex = -1; }
							}
						}
						else
						{
							ImGui::AlignTextToFramePadding();
							ImGui::TextUnformatted(waypoint.name.c_str());
						}

						ImGui::TableSetColumnIndex(1);
						
						int measure = accumulateMeasures(waypoint.tick, TICKS_PER_BEAT, context.score.timeSignatures) + 1;
						float time = accumulateDuration(waypoint.tick, TICKS_PER_BEAT, context.score.tempoChanges);
						char timeStr[64];
						// 小節の表示も日本語化
						snprintf(timeStr, sizeof(timeStr), "%s %d (%02d:%02d)", getString("measure"), measure, (int)time / 60, (int)std::fmod(time, 60.0f));

						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetStyle().ItemSpacing.x, startPos.y));
						
						if (isSelected) 
							ImGui::PushStyleColor(ImGuiCol_Text, activeTextColor);
						else 
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)); 
							
						ImGui::AlignTextToFramePadding();
						ImGui::Text("%s", timeStr);
						ImGui::PopStyleColor();

						ImGui::PopID();
					}
					ImGui::EndTable();
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
		}
		ImGui::End();
	}
}
