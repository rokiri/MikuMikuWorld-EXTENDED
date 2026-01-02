#pragma once
#include <string_view>

namespace MikuMikuWorld::Text
{
#define MMW_TEXT_2(NAME, KEY)                                                                      \
	constexpr static const char* NAME { KEY }
#define MMW_TEXT_1(NAME) MMW_TEXT_2(NAME, #NAME)
#define MMW_TEXT_SELECT(_1, _2, name, ...) name
#define MMW_EXP(txt) txt
#define MMW_TEXT(...) MMW_EXP(MMW_TEXT_SELECT(__VA_ARGS__, MMW_TEXT_2, MMW_TEXT_1)(__VA_ARGS__))

	// Localize text section
	// Use MMW_TEXT to declare a translation key
	// Try keep these strings in sync with the i18n files

	MMW_TEXT(languageName, "language_name");
	MMW_TEXT(translator);
	// Menu Bar
	MMW_TEXT(file);
	MMW_TEXT(newFile, "new");
	MMW_TEXT(openFile, "open");
	MMW_TEXT(openRecent, "open_recent");
	MMW_TEXT(clear);
	MMW_TEXT(save);
	MMW_TEXT(saveAs, "save_as");
	MMW_TEXT(exportScore, "export_score");
	MMW_TEXT(exit);
	MMW_TEXT(edit);
	MMW_TEXT(undo);
	MMW_TEXT(redo);
	MMW_TEXT(selectAll, "select_all");
	MMW_TEXT(view);
	MMW_TEXT(settings);
	MMW_TEXT(window);
	MMW_TEXT(vsync);
	MMW_TEXT(fpsShow, "show_fps");
	MMW_TEXT(debug);
	MMW_TEXT(createAutoSave, "create_auto_save");
	MMW_TEXT(help);
	MMW_TEXT(about);

	// Dialogs
	MMW_TEXT(fileNotFound, "file_not_found");
	MMW_TEXT(fileNotFoundMsg1, "file_not_found_msg1");
	MMW_TEXT(fileNotFoundMsg2, "file_not_found_msg2");
	MMW_TEXT(removeRecentFileNotFound, "remove_recent_file_not_found");
	MMW_TEXT(yes);
	MMW_TEXT(no);
	MMW_TEXT(unsavedChanges, "unsaved_changes");
	MMW_TEXT(askSave, "ask_save");
	MMW_TEXT(warnUnsaved, "warn_unsaved");
	MMW_TEXT(saveChanges, "save_changes");
	MMW_TEXT(discardChanges, "discard_changes");
	MMW_TEXT(updateAvailable, "update_available");
	MMW_TEXT(updateAvailableDescription, "update_available_description");
	MMW_TEXT(scoreFile, "score_file");
	MMW_TEXT(exportSuccessful, "export_successful");
	MMW_TEXT(unknownFileFormat, "unknown_file_format");
	MMW_TEXT(openAsFileFormat, "open_as_file_format");
	MMW_TEXT(exportAsFileFormat, "export_as_file_format");
	MMW_TEXT(error);
	MMW_TEXT(errorLoadScoreFile, "error_load_score_file");
	MMW_TEXT(errorSaveScoreFile, "error_save_score_file");
	MMW_TEXT(errorLoadMusicFile, "error_load_music_file");
	MMW_TEXT(scorePartiallyMissing, "score_partially_missing");
	MMW_TEXT(cancel);
	MMW_TEXT(general);
	MMW_TEXT(keyConfig, "key_config");
	MMW_TEXT(language);
	MMW_TEXT(minify);
	MMW_TEXT(automatic, "auto");
	MMW_TEXT(autoSave, "auto_save");
	MMW_TEXT(autoSaveEnable, "auto_save_enable");
	MMW_TEXT(autoSaveInterval, "auto_save_interval");
	MMW_TEXT(autoSaveCount, "auto_save_count");
	MMW_TEXT(accentColor, "accent_color");
	MMW_TEXT(accentColorHelp, "accent_color_help");
	MMW_TEXT(selectAccentColor, "select_accent_color");
	MMW_TEXT(displayMode, "display_mode");
	MMW_TEXT(theme);
	MMW_TEXT(baseTheme, "base_theme");
	MMW_TEXT(themeLight, "theme_light");
	MMW_TEXT(themeDark, "theme_dark");
	MMW_TEXT(customColor, "custom_color");
	MMW_TEXT(timeline);
	MMW_TEXT(matchNotesSizeToTimeline, "match_notes_size_to_timeline");
	MMW_TEXT(laneWidth, "lane_width");
	MMW_TEXT(notesHeight, "notes_height");
	MMW_TEXT(scrolling);
	MMW_TEXT(scrollSpeedNormal, "scroll_speed_normal");
	MMW_TEXT(scrollSpeedShift, "scroll_speed_shift");
	MMW_TEXT(useSmoothScroll, "use_smooth_scroll");
	MMW_TEXT(smoothScrollTime, "smooth_scroll_time");
	MMW_TEXT(returnToLastTick, "return_to_last_tick");
	MMW_TEXT(cursorAutoScroll, "cursor_auto_scroll");
	MMW_TEXT(cursorAutoScrollAmount, "cursor_auto_scroll_amount");
	MMW_TEXT(background);
	MMW_TEXT(backgroundImage, "background_image");
	MMW_TEXT(drawBackground, "draw_background");
	MMW_TEXT(backgroundBrightnes, "background_brightnes");
	MMW_TEXT(lanesOpacity, "lanes_opacity");
	MMW_TEXT(video);
	MMW_TEXT(notesSe, "notes_se");
	MMW_TEXT(showTickInProperties, "show_tick_in_properties");

	// score editor
	MMW_TEXT(options);
	MMW_TEXT(chartProperties, "chart_properties");
	MMW_TEXT(noteProperties, "note_properties");
	MMW_TEXT(notesTimeline, "notes_timeline");
	MMW_TEXT(select);
	MMW_TEXT(tap);
	MMW_TEXT(hold);
	MMW_TEXT(holdStep, "hold_step");
	MMW_TEXT(flick);
	MMW_TEXT(critical);
	MMW_TEXT(dummy);
	MMW_TEXT(holdDummy, "hold_dummy");
	MMW_TEXT(trace);
	MMW_TEXT(guide);
	MMW_TEXT(damage);
	MMW_TEXT(bpm);
	MMW_TEXT(timeSignature, "time_signature");
	MMW_TEXT(none);
	MMW_TEXT(defaultValue, "default");
	MMW_TEXT(left);
	MMW_TEXT(right);
	MMW_TEXT(down);
	MMW_TEXT(downLeft, "down_left");
	MMW_TEXT(downRight, "down_right");
	MMW_TEXT(normal);
	MMW_TEXT(hidden);
	MMW_TEXT(skip);
	MMW_TEXT(linear);
	MMW_TEXT(easeIn, "ease_in");
	MMW_TEXT(easeOut, "ease_out");
	MMW_TEXT(easeInOut, "ease_in_out");
	MMW_TEXT(easeOutIn, "ease_out_in");
	MMW_TEXT(del, "delete");
	MMW_TEXT(copy);
	MMW_TEXT(cut);
	MMW_TEXT(paste);
	MMW_TEXT(flipPaste, "flip_paste");
	MMW_TEXT(duplicate);
	MMW_TEXT(flipDuplicate, "flip_duplicate");
	MMW_TEXT(flip);
	MMW_TEXT(noteWidth, "note_width");
	MMW_TEXT(shrinkDown, "shrink_down");
	MMW_TEXT(shrinkUp, "shrink_up");
	MMW_TEXT(compressSelection, "compress_selection");
	MMW_TEXT(connectHolds, "connect_holds");
	MMW_TEXT(splitHold, "split_hold");
	MMW_TEXT(repeatHoldMids, "repeat_hold_mids");
	MMW_TEXT(holdToTraces, "hold_to_traces");
	MMW_TEXT(addTracesForHold, "add_traces_for_hold");
	MMW_TEXT(convertHoldToTraces, "convert_hold_to_traces");
	MMW_TEXT(convertGuideHold, "convert_guide_hold");
	MMW_TEXT(convertGuideToHold, "convert_guide_to_hold");
	MMW_TEXT(convertHoldToGuide, "convert_hold_to_guide");
	MMW_TEXT(lerpHispeeds, "lerp_hispeeds");
	MMW_TEXT(stepType, "step_type");
	MMW_TEXT(easeType, "ease_type");
	MMW_TEXT(flickType, "flick_type");
	MMW_TEXT(holdType, "hold_type");
	MMW_TEXT(fadeType, "fade_type");
	MMW_TEXT(holdStartType, "hold_start_type");
	MMW_TEXT(holdEndType, "hold_end_type");
	MMW_TEXT(metadata);
	MMW_TEXT(title);
	MMW_TEXT(designer);
	MMW_TEXT(artist);
	MMW_TEXT(jacket);
	MMW_TEXT(laneExtension, "lane_extension");
	MMW_TEXT(audio);
	MMW_TEXT(musicFile, "music_file");
	MMW_TEXT(musicOffset, "music_offset");
	MMW_TEXT(volumeMaster, "volume_master");
	MMW_TEXT(volumeBgm, "volume_bgm");
	MMW_TEXT(volumeSe, "volume_se");
	MMW_TEXT(statistics);
	MMW_TEXT(hispeeds);
	MMW_TEXT(taps);
	MMW_TEXT(flicks);
	MMW_TEXT(holds);
	MMW_TEXT(steps);
	MMW_TEXT(guides);
	MMW_TEXT(traces);
	MMW_TEXT(total);
	MMW_TEXT(combo);
	MMW_TEXT(gotoMeasure, "goto_measure");
	MMW_TEXT(division);
	MMW_TEXT(divisionAffix, "division_affix");
	MMW_TEXT(custom);
	MMW_TEXT(customDivision, "custom_division");
	MMW_TEXT(snapMode, "snap_mode");
	MMW_TEXT(snapModeRelative, "snap_mode_relative");
	MMW_TEXT(snapModeAbsolute, "snap_mode_absolute");
	MMW_TEXT(snapModeIndividualAbsolute, "snap_mode_individual_absolute");
	MMW_TEXT(zoom);
	MMW_TEXT(showStepOutlines, "show_step_outlines");
	MMW_TEXT(drawWaveform, "draw_waveform");
	MMW_TEXT(editBpm, "edit_bpm");
	MMW_TEXT(tick);
	MMW_TEXT(remove);
	MMW_TEXT(editTimeSignature, "edit_time_signature");
	MMW_TEXT(measure);
	MMW_TEXT(skill);
	MMW_TEXT(increaseNoteSize, "increase_note_size");
	MMW_TEXT(decreaseNoteSize, "decrease_note_size");
	MMW_TEXT(hiSpeed, "hi_speed");
	MMW_TEXT(editHiSpeed, "edit_hi_speed");
	MMW_TEXT(hiSpeedSpeed, "hi_speed_speed");
	MMW_TEXT(hiSpeedSkipBeat, "hi_speed_skip_beat");
	MMW_TEXT(hiSpeedEase, "hi_speed_ease");
	MMW_TEXT(hiSpeedEaseNone, "hi_speed_ease_none");
	MMW_TEXT(hiSpeedEaseLinear, "hi_speed_ease_linear");
	MMW_TEXT(hiSpeedHideNotes, "hi_speed_hide_notes");
	MMW_TEXT(fadeNone, "fade_none");
	MMW_TEXT(fadeIn, "fade_in");
	MMW_TEXT(fadeOut, "fade_out");
	MMW_TEXT(guideColor, "guide_color");
	MMW_TEXT(guideNeutral, "guide_neutral");
	MMW_TEXT(guideRed, "guide_red");
	MMW_TEXT(guideBlue, "guide_blue");
	MMW_TEXT(guideGreen, "guide_green");
	MMW_TEXT(guideYellow, "guide_yellow");
	MMW_TEXT(guidePurple, "guide_purple");
	MMW_TEXT(guideCyan, "guide_cyan");
	MMW_TEXT(guideBlack, "guide_black");
	MMW_TEXT(layer);
	MMW_TEXT(editSkill, "edit_skill");
	MMW_TEXT(editFever, "edit_fever");
	MMW_TEXT(editNone, "edit_none");
	MMW_TEXT(editEvent, "edit_event");
	MMW_TEXT(beat);
	MMW_TEXT(lane);
	MMW_TEXT(width);
	MMW_TEXT(advanced);
	MMW_TEXT(notePropertiesNote, "note_properties_note");
	MMW_TEXT(notePropertiesHiSpeed, "note_properties_hi_speed");
	MMW_TEXT(notePropertiesHoldNote, "note_properties_hold_note");
	MMW_TEXT(notePropertiesGuide, "note_properties_guide");
	MMW_TEXT(notePropertiesNotSelected, "note_properties_not_selected");
	MMW_TEXT(notePropertiesManyHoldNotes, "note_properties_many_hold_notes");
	MMW_TEXT(translatedBy, "translated_by");
	MMW_TEXT(zoomTooltip, "zoom_tooltip");

	// preset manager
	MMW_TEXT(presets);
	MMW_TEXT(noPresets, "no_presets");
	MMW_TEXT(presetName, "preset_name");
	MMW_TEXT(description);
	MMW_TEXT(createPreset, "create_preset");
	MMW_TEXT(search);
	MMW_TEXT(confirm);

	// layer manager
	MMW_TEXT(layers);
	MMW_TEXT(layerName, "layer_name");
	MMW_TEXT(createLayer, "create_layer");
	MMW_TEXT(showAllLayers, "show_all_layers");
	MMW_TEXT(layerRename, "layer_rename");
	MMW_TEXT(layerUp, "layer_up");
	MMW_TEXT(layerDown, "layer_down");
	MMW_TEXT(layerMerge, "layer_merge");
	MMW_TEXT(layerDelete, "layer_delete");
	MMW_TEXT(layerHide, "layer_hide");
	MMW_TEXT(layerShow, "layer_show");

	// waypoint manager
	MMW_TEXT(waypoints);
	MMW_TEXT(createWaypoint, "create_waypoint");
	MMW_TEXT(editWaypoint, "edit_waypoint");
	MMW_TEXT(waypointName, "waypoint_name");

	// commands
	MMW_TEXT(action);
	MMW_TEXT(keys);
	MMW_TEXT(add);
	MMW_TEXT(cmdKeyListen, "cmd_key_listen");
	MMW_TEXT(newChart, "new_chart");
	MMW_TEXT(togglePlayback, "toggle_playback");
	MMW_TEXT(cancelPaste, "cancel_paste");
	MMW_TEXT(stop);
	MMW_TEXT(previousTick, "previous_tick");
	MMW_TEXT(nextTick, "next_tick");
	MMW_TEXT(timelineSelect, "timeline_select");
	MMW_TEXT(timelineTap, "timeline_tap");
	MMW_TEXT(timelineHold, "timeline_hold");
	MMW_TEXT(timelineHoldStep, "timeline_hold_step");
	MMW_TEXT(timelineFlick, "timeline_flick");
	MMW_TEXT(timelineCritical, "timeline_critical");
	MMW_TEXT(timelineTrace, "timeline_trace");
	MMW_TEXT(timelineGuide, "timeline_guide");
	MMW_TEXT(timelineDamage, "timeline_damage");
	MMW_TEXT(timelineDummy, "timeline_dummy");
	MMW_TEXT(timelineBpm, "timeline_bpm");
	MMW_TEXT(timelineTimeSignature, "timeline_time_signature");
	MMW_TEXT(timelineHiSpeed, "timeline_hi_speed");
	MMW_TEXT(zoomIn, "zoom_in");
	MMW_TEXT(zoomOut, "zoom_out");
#undef MMW_TEXT_2
#undef MMW_TEXT_1
#undef MMW_TEXT_FILTER
#undef MMW_EXP
#undef MMW_TEXT
}