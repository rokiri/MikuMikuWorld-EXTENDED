#pragma once
#include "Constants.h"
#include "NoteTypes.h"
#include <string>

namespace MikuMikuWorld
{
	id_t getNextStageID();
	id_t getNextCameraChangeID();
	id_t getNextStageMaskChangeID();
	id_t getNextStagePivotChangeID();
	id_t getNextStageStyleChangeID();
	id_t getNextStageTransformChangeID();

	enum class StageZoomVerticalAlign : uint8_t
	{
		Default,
		Center
	};

	constexpr const char* stageZoomVerticalAlignNames[]{ "Default", "Center" };

	enum class StageBorderStyle : uint8_t
	{
		Default,
		Light,
		Disabled,
		Medium
	};

	constexpr const char* stageBorderStyleNames[]{ "Default", "Light", "Disabled", "Medium" };

	struct Stage
	{
		id_t ID;
		std::string editorName;
		bool fromStart = true;
		bool untilEnd = true;
		bool generateSimLinesIsolated = false;
	};

	struct CameraChangeEvent
	{
		id_t ID;
		int tick;
		float left = 0;
		float size = 12;
		float zoom = 1;
		float zoomTargetLane = 0;
		float zoomTargetY = 0;
		StageZoomVerticalAlign zoomVerticalAlign = StageZoomVerticalAlign::Default;
		float rotate = 0;
		float stageTilt = 1;
		EaseType ease = EaseType::Linear;
	};

	struct StageMaskChangeEvent
	{
		id_t ID;
		id_t stageID = NO_ID;
		int tick;
		float left = 0;
		float size = 12;
		EaseType ease = EaseType::Linear;
	};

	struct StagePivotChangeEvent
	{
		id_t ID;
		id_t stageID = NO_ID;
		int tick;
		float lane = 0;
		int divisionSize = 2;
		bool divisionParityOdd = false;
		float yOffset = 0;
		float yOffsetBeat = 0;
		EaseType ease = EaseType::Linear;
	};

	struct StageStyleChangeEvent
	{
		id_t ID;
		id_t stageID;
		int tick;
		GuideColor judgeLineColor = GuideColor::Purple;
		StageBorderStyle leftBorderStyle = StageBorderStyle::Default;
		StageBorderStyle rightBorderStyle = StageBorderStyle::Default;
		float alpha = 1;
		float laneAlpha = 1;
		float judgeLineAlpha = 1;
		EaseType ease = EaseType::Linear;
	};

	// nih stage transform

	enum class StageTransformAnchor : uint8_t
	{
		Default,
		Center
	};
	constexpr const char* stageTransformAnchorNames[]{ "Default", "Center" };

	struct StageTransformEvent
	{
		id_t ID;
		id_t stageID = NO_ID;
		int tick;
		float rotate = 0;
		float xLaneTranslate = 0;
		float yLaneTranslate = 0;
		StageTransformAnchor anchor = StageTransformAnchor::Default;
		EaseType ease = EaseType::Linear;
	};

	enum class StageEventEditMode : uint8_t
	{
		None,
		Camera,
		Mask,
		Pivot,
		Style,
		Transform,
		StageEventEditModeMax
	};

	constexpr const char* stageEventEditModeNames[]{ "None",  "Camera", "Mask",
		                                             "Pivot", "Style",  "Transform" };
}