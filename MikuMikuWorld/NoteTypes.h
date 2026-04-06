#pragma once
#include <cstdint>
#include <utility>
#include "Enum.h"

namespace MikuMikuWorld
{
	enum class NoteType : uint8_t
	{
		Tap,
		Tick,
		Damage,
		NoteTypeCount
	};

	enum class NoteFlag : uint8_t
	{
		None,
		Critical = 1 << 0,
		Trace = 1 << 1,
		Dummy = 1 << 2,
		Attached = 1 << 3,
		Hidden = 1 << 4,
		LongNote = 1 << 6, // Use for rendering, has no other effect
		NonAttached = 1 << 7, // Override Attached flag, for case where it need to be preserved
	};
	DECLARE_ENUM_FLAG_OPERATORS(NoteFlag)

	enum class FlickType : uint8_t
	{
		None,
		Default,
		Left,
		Right,
		Down,
		DownLeft,
		DownRight,
		FlickTypeCount
	};

	inline constexpr const char* flickTypes[]{ "none", "default",   "left",      "right",
		                                       "down", "down_left", "down_right" };


	enum class EaseType : uint8_t
	{
		Linear,
		EaseIn,
		EaseOut,
		EaseInOut,
		EaseOutIn,
		EaseNone,
		EaseTypeCount
	};

	inline constexpr const char* easeTypes[]{ "linear", "in", "out", "inout", "outin", "none" };

	enum class HoldNoteFlag : uint16_t
	{
		Normal = 0,
		Critical = 1 << 0,
		Dummy = 1 << 1,
		Guide = 1 << 2,
	};
	DECLARE_ENUM_FLAG_OPERATORS(HoldNoteFlag)

	// constexpr const char* holdTypes[]{ "normal", "hidden", "guide" };

	enum class EditHoldJointType : uint8_t
	{
		Normal,
		Trace,
		Hidden,
		JointTypeCount
	};

	inline constexpr const char* holdEndTypes[]{ "normal", "trace", "hidden" };

	enum class EditHoldStepType : uint8_t
	{
		Normal,
		Hidden,
		Skip,
		HoldStepTypeCount
	};

	inline constexpr const char* stepTypes[]{ "normal", "hidden", "skip" };

	enum class ClassicGuideColor : uint8_t
	{
		Green,
		Yellow,
		GuideColorCount
	};

	enum class GuideColor : uint8_t
	{
		Neutral,
		Red,
		Green,
		Blue,
		Yellow,
		Purple,
		Cyan,
		Black,
		GuideColorCount
	};

	inline constexpr const char* guideColors[]{ "neutral", "red",    "green", "blue",
		                                        "yellow",  "purple", "cyan",  "black" };

	enum class FadeType : uint8_t
	{
		Out,
		None,
		In,
		Custom,
		Classic,
		FadeTypeCount
	};

	inline constexpr const char* fadeTypes[] = { "out", "none", "in", "custom", "default" };

	enum class HiSpeedEaseType : uint8_t
	{
		None,
		Linear,
		EaseTypeCount
	};

	using id_t = int32_t;
	using tick_t = int32_t;
	using beat_t = float;
	using qnote_t = float;
	using secs_t = double;
	using measure_t = int32_t;
	using layered_tick_t = std::pair<id_t, tick_t>;
}
