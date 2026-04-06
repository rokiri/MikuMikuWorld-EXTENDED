#include "Math.h"

namespace MikuMikuWorld
{
	float lerp(float start, float end, float percentage)
	{
		return start + percentage * (end - start);
	}

	float unlerp(float start, float end, float value, float fallback)
	{
		return end != start ? (value - start) / (end - start) : fallback;
	}

	double lerpD(double start, double end, double percentage)
	{
		return start + percentage * (end - start);
	}

	double unlerpD(double start, double end, double value, double fallback)
	{
		return end != start ? (value - start) / (end - start) : fallback;
	}

	float easeIn(float start, float end, float ratio)
	{
		//
		return lerp(start, end, ratio * ratio);
	}

	float easeOut(float start, float end, float ratio)
	{
		return lerp(start, end, 1 - (1 - ratio) * (1 - ratio));
	}

	float easeInOut(float start, float end, float ratio)
	{
		if (ratio < 0.5)
		{
			return easeIn(start, midpoint(start, end), ratio * 2);
		}
		else
		{
			return easeOut(midpoint(start, end), end, ratio * 2 - 1);
		}
	}

	float easeOutIn(float start, float end, float ratio)
	{
		if (ratio < 0.5)
		{
			return easeOut(start, midpoint(start, end), ratio * 2);
		}
		else
		{
			return easeIn(midpoint(start, end), end, ratio * 2 - 1);
		}
	}

	float easeNone(float start, float, float) { return start; }

	float midpoint(float x1, float x2) { return (x1 + x2) * 0.5f; }

	bool isWithinRange(float x, float left, float right) { return x >= left && x <= right; }

	EaseFunction getEaseFunction(EaseType ease)
	{
		switch (ease)
		{
		case EaseType::Linear:
			return lerp;
		case EaseType::EaseIn:
			return easeIn;
		case EaseType::EaseOut:
			return easeOut;
		case EaseType::EaseInOut:
			return easeInOut;
		case EaseType::EaseOutIn:
			return easeOutIn;
		case EaseType::EaseNone:
			return easeNone;
		default:
			assert(false && "EaseType is not supported!");
			break;
		}

		return lerp;
	}

	uint32_t gcf(uint32_t a, uint32_t b)
	{
		for (;;)
		{
			if (b == 0)
			{
				break;
			}
			else
			{
				uint32_t t = a;
				a = b;
				b = t % a;
			}
		}

		return a;
	}
}