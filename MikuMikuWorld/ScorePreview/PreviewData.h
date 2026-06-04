#pragma once
#include <array>
#include <set>
#include <map>
#include <vector>
#include "../Score.h"
#include "../Math.h"

namespace MikuMikuWorld
{
    struct Score;
    struct Note;
    struct ScoreContext;
}

namespace MikuMikuWorld::Engine
{
    struct Range
    {
        double min;
        double max;
    };

    struct DrawingNote
    {
        int refID;
        Range visualTime;
    };

    struct DrawingLine
    {
        Range xPos;
        Range visualTime;
    };

    struct DrawingHoldTick
    {
        int refID;
        float center;
        Range visualTime;
    };

    struct DrawingHoldSegment
    {
        int endID;
        int startID;
        EaseType ease;
        bool isGuide;
        ptrdiff_t tailStepIndex;
        double headTime, tailTime;
        float headLeft, headRight;
        float tailLeft, tailRight;
        float startTime, endTime;
        double activeTime;
    };

    struct DrawData
    {
        float noteSpeed = 8.0f;
        int maxTicks = 1;

        std::vector<DrawingNote>        drawingNotes;
        std::vector<DrawingLine>        drawingLines;
        std::vector<DrawingHoldTick>    drawingHoldTicks;
        std::vector<DrawingHoldSegment> drawingHoldSegments;

        void clear();
        void calculateDrawData(const Score& score);
    };
}
