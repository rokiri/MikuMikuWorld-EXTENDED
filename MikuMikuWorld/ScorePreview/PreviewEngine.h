#pragma once
#include <cmath>
#include "../Math.h"
#include "../Score.h"
#include "PreviewData.h"

namespace MikuMikuWorld::Engine
{
    static inline float getNoteDuration(float noteSpeed)
    {
        return (float)lerpD(0.35, 4.0, std::pow(unlerpD(12, 1, noteSpeed), 1.31));
    }

    static inline double approach(double start_time, double end_time, double current_time)
    {
        return std::pow(1.06, 45 * lerpD(-1, 0, unlerpD(start_time, end_time, current_time)));
    }

    inline constexpr float STAGE_LANE_TOP    = 47.f;
    inline constexpr float STAGE_LANE_BOTTOM = 803.f;
    inline constexpr float STAGE_LANE_HEIGHT = 850.f;
    inline constexpr float STAGE_LANE_WIDTH  = 1420.f;
    inline constexpr float STAGE_NUM_LANES   = 12.f;
    inline constexpr float STAGE_TEX_WIDTH   = 2048.f;
    inline constexpr float STAGE_TEX_HEIGHT  = 1176.f;
    inline constexpr float STAGE_NOTE_HEIGHT = 75.f;
    inline constexpr float STAGE_TARGET_WIDTH  = 1920.f;
    inline constexpr float STAGE_TARGET_HEIGHT = 1080.f;
    inline constexpr float STAGE_ASPECT_RATIO  = STAGE_TARGET_WIDTH / STAGE_TARGET_HEIGHT;
    inline constexpr float STAGE_ZOOM = 927 / 800.f;
    inline constexpr float BACKGROUND_SIZE = 2462.25f;

    inline constexpr float STAGE_WIDTH_RATIO =
        STAGE_ZOOM * STAGE_LANE_WIDTH / (STAGE_TEX_HEIGHT * STAGE_ASPECT_RATIO) / STAGE_NUM_LANES;
    inline constexpr float STAGE_HEIGHT_RATIO =
        STAGE_ZOOM * STAGE_LANE_HEIGHT / STAGE_TEX_HEIGHT;
    inline constexpr float STAGE_TOP_RATIO =
        0.5f + STAGE_ZOOM * STAGE_LANE_TOP / STAGE_TEX_HEIGHT;

    static inline float laneToLeft(float lane)
    {
        return lane - 6.f;
    }

    static inline float getNoteCenter(const Note& note)
    {
        return laneToLeft(note.lane) + note.width / 2.f;
    }

    static inline float getNoteHeight()
    {
        return STAGE_NOTE_HEIGHT / STAGE_LANE_HEIGHT / 2.f;
    }

    inline Range getNoteVisualTime(const Note& note, const Score& score, float noteSpeed)
    {
        double targetTime = accumulateScaledDuration(note.tick, score);
        return Range{ targetTime - getNoteDuration(noteSpeed), targetTime };
    }
}
