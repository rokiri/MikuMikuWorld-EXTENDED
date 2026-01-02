#pragma once
#include "InputBinding.h"
#include "Text.h"
#include "ImGui/imgui.h"

namespace MikuMikuWorld
{
	struct InputConfiguration
	{
		MultiInputBinding togglePlayback = { Text::togglePlayback,
			                                 { ImGuiKey_Space, ImGuiMod_None } };
		MultiInputBinding stop = { Text::stop, { ImGuiKey_Backspace } };
		MultiInputBinding decreaseNoteSize = { Text::decreaseNoteSize, {} };
		MultiInputBinding increaseNoteSize = { Text::increaseNoteSize, {} };
		MultiInputBinding shrinkDown = { Text::shrinkDown, {} };
		MultiInputBinding shrinkUp = { Text::shrinkUp, {} };
		MultiInputBinding compressSelection = { Text::compressSelection, {} };
		MultiInputBinding connectHolds = { Text::connectHolds, {} };
		MultiInputBinding splitHold = { Text::splitHold, {} };
		MultiInputBinding lerpHiSpeeds = { Text::lerpHispeeds, {} };
		MultiInputBinding openHelp = { Text::help, { ImGuiKey_F1 } };
		MultiInputBinding openSettings = { Text::settings, { ImGuiKey_Comma, ImGuiMod_Ctrl } };
		MultiInputBinding deleteSelection = { Text::del, { ImGuiKey_Delete } };
		MultiInputBinding copySelection = { Text::copy, { ImGuiKey_C, ImGuiMod_Ctrl } };
		MultiInputBinding cutSelection = { Text::cut, { ImGuiKey_X, ImGuiMod_Ctrl } };
		MultiInputBinding paste = { Text::paste, { ImGuiKey_V, ImGuiMod_Ctrl } };
		MultiInputBinding flipPaste = { Text::flipPaste,
			                            { ImGuiKey_V, ImGuiMod_Ctrl | ImGuiMod_Shift } };
		MultiInputBinding duplicate = { Text::duplicate, { ImGuiKey_D, ImGuiMod_Ctrl } };
		MultiInputBinding flipDuplicate = { Text::flipDuplicate,
			                                { ImGuiKey_D, ImGuiMod_Ctrl | ImGuiMod_Shift } };
		MultiInputBinding flip = { Text::flip, { ImGuiKey_F, ImGuiMod_Ctrl } };
		MultiInputBinding cancelPaste = { Text::cancelPaste, { ImGuiKey_Escape } };
		MultiInputBinding previousTick = { Text::previousTick, { ImGuiKey_DownArrow } };
		MultiInputBinding nextTick = { Text::nextTick, { ImGuiKey_UpArrow } };
		MultiInputBinding create = { Text::newChart, { ImGuiKey_N, ImGuiMod_Ctrl } };
		MultiInputBinding open = { Text::openFile, { ImGuiKey_O, ImGuiMod_Ctrl } };
		MultiInputBinding save = { Text::save, { ImGuiKey_S, ImGuiMod_Ctrl } };
		MultiInputBinding saveAs = { Text::saveAs, { ImGuiKey_S, ImGuiMod_Ctrl | ImGuiMod_Shift } };
		MultiInputBinding exportScore = { Text::exportScore, { ImGuiKey_E, ImGuiMod_Ctrl } };
		MultiInputBinding selectAll = { Text::selectAll, { ImGuiKey_A, ImGuiMod_Ctrl } };
		MultiInputBinding undo = { Text::undo, { ImGuiKey_Z, ImGuiMod_Ctrl } };
		MultiInputBinding redo = { Text::redo, { ImGuiKey_Y, ImGuiMod_Ctrl } };
		MultiInputBinding zoomOut = { Text::zoomOut, {} };
		MultiInputBinding zoomIn = { Text::zoomIn, {} };

		MultiInputBinding timelineSelect = { Text::timelineSelect,
			                                 { ImGuiKey_1 },
			                                 { ImGuiKey_Keypad1 } };
		MultiInputBinding timelineTap = { Text::timelineTap, { ImGuiKey_2 }, { ImGuiKey_Keypad2 } };
		MultiInputBinding timelineHold = { Text::timelineHold,
			                               { ImGuiKey_3 },
			                               { ImGuiKey_Keypad3 } };
		MultiInputBinding timelineHoldMid = { Text::timelineHoldStep,
			                                  { ImGuiKey_4 },
			                                  { ImGuiKey_Keypad4 } };
		MultiInputBinding timelineFlick = { Text::timelineFlick,
			                                { ImGuiKey_5 },
			                                { ImGuiKey_Keypad5 } };
		MultiInputBinding timelineCritical = { Text::timelineCritical,
			                                   { ImGuiKey_6 },
			                                   { ImGuiKey_Keypad6 } };
		MultiInputBinding timelineFriction = { Text::timelineTrace,
			                                   { ImGuiKey_7 },
			                                   { ImGuiKey_Keypad7 } };
		MultiInputBinding timelineGuide = { Text::timelineGuide, {} };
		MultiInputBinding timelineDamage = { Text::timelineDamage, {} };
		MultiInputBinding timelineDummy = { Text::timelineDummy, {} };
		MultiInputBinding timelineBpm = { Text::timelineBpm, { ImGuiKey_8 }, { ImGuiKey_Keypad8 } };
		MultiInputBinding timelineTimeSignature = { Text::timelineTimeSignature,
			                                        { ImGuiKey_9 },
			                                        { ImGuiKey_Keypad9 } };
		MultiInputBinding timelineHiSpeed = { Text::timelineHiSpeed,
			                                  { ImGuiKey_0 },
			                                  { ImGuiKey_Keypad0 } };
	};

	inline constexpr MultiInputBinding InputConfiguration::* allInputBindings[] = {
		// Menu bar bindings
		&InputConfiguration::create,
		&InputConfiguration::open,
		&InputConfiguration::save,
		&InputConfiguration::saveAs,
		&InputConfiguration::exportScore,
		&InputConfiguration::undo,
		&InputConfiguration::redo,
		// Context menu bindings
		&InputConfiguration::cutSelection,
		&InputConfiguration::copySelection,
		&InputConfiguration::paste,
		&InputConfiguration::flipPaste,
		&InputConfiguration::duplicate,
		&InputConfiguration::flipDuplicate,
		&InputConfiguration::flip,
		&InputConfiguration::cancelPaste,
		&InputConfiguration::deleteSelection,
		&InputConfiguration::shrinkDown,
		&InputConfiguration::shrinkUp,
		&InputConfiguration::compressSelection,
		&InputConfiguration::connectHolds,
		&InputConfiguration::splitHold,
		&InputConfiguration::lerpHiSpeeds,
		&InputConfiguration::selectAll,
		// Timeline bindings
		&InputConfiguration::togglePlayback,
		&InputConfiguration::stop,
		&InputConfiguration::previousTick,
		&InputConfiguration::nextTick,
		&InputConfiguration::decreaseNoteSize,
		&InputConfiguration::increaseNoteSize,
		&InputConfiguration::zoomOut,
		&InputConfiguration::zoomIn,
		// Toolbar bindings
		&InputConfiguration::timelineSelect,
		&InputConfiguration::timelineTap,
		&InputConfiguration::timelineHold,
		&InputConfiguration::timelineHoldMid,
		&InputConfiguration::timelineFlick,
		&InputConfiguration::timelineCritical,
		&InputConfiguration::timelineFriction,
		&InputConfiguration::timelineGuide,
		&InputConfiguration::timelineDamage,
		&InputConfiguration::timelineDummy,
		&InputConfiguration::timelineBpm,
		&InputConfiguration::timelineTimeSignature,
		&InputConfiguration::timelineHiSpeed,
		// Misc bindings
		&InputConfiguration::openHelp,
		&InputConfiguration::openSettings,
	};

	inline constexpr MultiInputBinding InputConfiguration::* insertModeBindings[] = {
		&InputConfiguration::timelineSelect,   &InputConfiguration::timelineTap,
		&InputConfiguration::timelineHold,     &InputConfiguration::timelineHoldMid,
		&InputConfiguration::timelineFlick,    &InputConfiguration::timelineCritical,
		&InputConfiguration::timelineFriction, &InputConfiguration::timelineGuide,
		&InputConfiguration::timelineDamage,   &InputConfiguration::timelineDummy,
		&InputConfiguration::timelineBpm,      &InputConfiguration::timelineTimeSignature,
		&InputConfiguration::timelineHiSpeed,
	};
}