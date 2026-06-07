#pragma once

namespace MikuMikuWorld
{
    struct ScoreContext;
}

namespace MikuMikuWorld::Effect
{
    class EffectView
    {
    public:
        void reset() {}
        void init() {}
        void update(const MikuMikuWorld::ScoreContext&) {}
        bool isInitialized() const { return false; }
        bool isNoteEffectPlayed(int) const { return false; }
    };
}