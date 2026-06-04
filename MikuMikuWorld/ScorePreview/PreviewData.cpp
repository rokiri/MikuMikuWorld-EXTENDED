#include "PreviewData.h"
#include "../ScorePreview/PreviewEngine.h"
#include "../Tempo.h"
#include "../Constants.h"
#include "../ApplicationConfiguration.h"

namespace MikuMikuWorld::Engine
{
    struct DrawingHoldStep
    {
        tick_t tick;
        double time;
        float  left;
        float  right;
        EaseType ease;
    };

    static DrawingHoldStep makeStep(const Note& note, EaseType ease, const Score& score)
    {
        return DrawingHoldStep{
            note.tick,
            accumulateScaledDuration(note.tick, score),
            laneToLeft(note.lane),
            laneToLeft(note.lane) + note.width,
            ease
        };
    }

    static bool holdIsGuide(const HoldNote& hold)
    {
        return !hold.separators.empty() && hold.separators.front().isGuide();
    }

    static void addHoldNote(DrawData& drawData, const HoldNote& hold, const Score& score)
    {
        if (hold.steps.size() < 2)
            return;

        const float noteDuration = getNoteDuration(drawData.noteSpeed);
        const Note& startNote = score.notes.at(hold.steps.front());

        float activeTime  = (float)accumulateDuration(startNote.tick, score.tempoChanges);
        float startTime   = activeTime;
        DrawingHoldStep head = makeStep(startNote, startNote.ease, score);

        for (int tailIdx = 1; tailIdx < (int)hold.steps.size(); ++tailIdx)
        {
            const Note& tailNote = score.notes.at(hold.steps[tailIdx]);

            if (tailNote.isAttached())
            {
                double tickTime = accumulateScaledDuration(tailNote.tick, score);
                drawData.drawingHoldTicks.push_back(DrawingHoldTick{
                    tailNote.ID,
                    laneToLeft(tailNote.lane) + tailNote.width / 2.f,
                    Range{ tickTime - noteDuration, tickTime }
                });
                continue;
            }

            DrawingHoldStep tail = makeStep(tailNote, tailNote.ease, score);
            float endTime = (float)accumulateDuration(tailNote.tick, score.tempoChanges);
            const Note& endNote = score.notes.at(hold.steps.back());

            drawData.drawingHoldSegments.push_back(DrawingHoldSegment{
                endNote.ID,
                startNote.ID,
                head.ease,
                holdIsGuide(hold),
                tailIdx,
                head.time, tail.time,
                head.left, head.right,
                tail.left, tail.right,
                startTime, endTime,
                activeTime
            });

            startTime = endTime;

            if (!tailNote.isHidden() && tailIdx < (int)hold.steps.size() - 1)
            {
                double tickTime = accumulateScaledDuration(tailNote.tick, score);
                drawData.drawingHoldTicks.push_back(DrawingHoldTick{
                    tailNote.ID,
                    getNoteCenter(tailNote),
                    Range{ tickTime - noteDuration, tickTime }
                });
            }

            head = tail;
        }
    }

    void DrawData::clear()
    {
        drawingNotes.clear();
        drawingLines.clear();
        drawingHoldTicks.clear();
        drawingHoldSegments.clear();
        maxTicks = 1;
    }

    void DrawData::calculateDrawData(const Score& score)
    {
        clear();
        noteSpeed = getConfig().pvNoteSpeed;

        std::map<tick_t, Range> simBuilder;

        for (const auto& [id, note] : score.notes)
        {
            maxTicks = std::max(note.tick, maxTicks);

            if (note.isHold())
            {
                const HoldNote& hold = score.holdNotes.at(note.holdID);
                bool isStart = hold.steps.front() == id;
                bool isEnd   = hold.steps.back()  == id;
                if (!isStart && !isEnd) continue;
                if (isStart && note.isHidden()) continue;
                if (isEnd   && note.isHidden()) continue;
            }

            if (note.type == NoteType::Damage)
                continue;

            auto visualTime = getNoteVisualTime(note, score, noteSpeed);
            drawingNotes.push_back(DrawingNote{ note.ID, visualTime });

            float center = getNoteCenter(note);
            auto [it, inserted] = simBuilder.try_emplace(note.tick, Range{ center, center });
            if (!inserted)
            {
                it->second.min = std::min(it->second.min, (double)center);
                it->second.max = std::max(it->second.max, (double)center);
            }
        }

        for (const auto& [tick, xRange] : simBuilder)
        {
            if (xRange.min != xRange.max)
            {
                double targetTime = accumulateScaledDuration(tick, score);
                drawingLines.push_back(DrawingLine{
                    xRange,
                    Range{ targetTime - getNoteDuration(noteSpeed), targetTime }
                });
            }
        }

        for (const auto& [id, hold] : score.holdNotes)
            addHoldNote(*this, hold, score);
    }
}
