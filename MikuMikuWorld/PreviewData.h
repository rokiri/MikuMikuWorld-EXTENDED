#pragma once
#include <random>
#include "Score.h"
#include "Math.h"
#include "EffectView.h"

namespace MikuMikuWorld
{
	struct Score;
	struct Note;
	struct ScoreContext;
}

namespace MikuMikuWorld::Engine
{
	struct DrawingNote
	{
		int refID;
		Range visualTime;
		NoteType type;
		bool dummy;
		int layer;
	};

	// 縲蝉ｿｮ豁｣迚医大ｮ滓ｩ滉ｻ墓ｧ倥↓蜷医ｏ縺帙◆蜷梧凾謚ｼ縺礼ｷ壹・繝・・繧ｿ讒矩
	struct DrawingLine
	{
		int leftTick;
		float leftLane;
		int leftLayer;

		int rightTick;
		float rightLane;
		int rightLayer;
	};

	struct DrawingHoldTick
	{
		int refID;
		float center;
		Range visualTime;
		bool dummy;
		int layer;
	};

	struct DrawingHoldSegment
	{
		int endID;
		EaseType ease;
		bool isGuide;
		GuideColor color; // 霑ｽ蜉: 繧ｬ繧､繝臥ｷ壹・濶ｲ
		bool dummy;       // 霑ｽ蜉
		int layer;        // 霑ｽ蜉

		ptrdiff_t tailStepIndex;
		double headTime, tailTime;
		float headLeft, headRight;
		float tailLeft, tailRight;
		float startTime, endTime;
		double activeTime;

		int startTick;
		int endTick;
	};

	struct HiSpeedCacheNode
	{
		int tick;
		double stm;
		double speedPerTick;
	};

	struct LayerHiSpeedCache
	{
		std::vector<HiSpeedCacheNode> nodes;
		double getStm(int tick) const;
	};

	struct DrawData
	{
		float noteSpeed;
		int maxTicks;
		std::vector<DrawingNote> drawingNotes;
		std::vector<DrawingLine> drawingLines;
		std::vector<DrawingHoldTick> drawingHoldTicks;
		std::vector<DrawingHoldSegment> drawingHoldSegments;

		std::vector<LayerHiSpeedCache> hsCache;

		Effect::EffectView effectView;

		void clear();
		void calculateDrawData(Score const& score);
	};
}
