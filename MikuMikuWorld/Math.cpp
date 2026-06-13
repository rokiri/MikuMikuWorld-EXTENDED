#include "Math.h"
#include <stdexcept>

namespace MikuMikuWorld
{
	float lerp(float start, float end, float percentage)
	{
		return start + percentage * (end - start);
	}

	float unlerp(float start, float end, float value)
	{
		if (end != start)
			return (value - start) / (end - start);
		else
			return 0;
	}

	double lerpD(double start, double end, double percentage)
	{
		return start + percentage * (end - start);
	}

	double unlerpD(double start, double end, double value)
	{
		if (end != start)
			return (value - start) / (end - start);
		else
			return 0;
	}

	float easeIn(float start, float end, float ratio) { return lerp(start, end, ratio * ratio); }

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

	float midpoint(float x1, float x2) { return (x1 + x2) * 0.5f; }

	bool isWithinRange(float x, float left, float right) { return x >= left && x <= right; }

	std::function<float(float, float, float)> getEaseFunction(EaseType ease)
	{
		switch (ease)
		{
		case EaseType::EaseIn:
			return easeIn;
		case EaseType::EaseOut:
			return easeOut;
		case EaseType::EaseInOut:
			return easeInOut;
		case EaseType::EaseOutIn:
			return easeOutIn;
		default:
			break;
		}

		return lerp;
	}

	std::tuple<Vector2, Vector2, Vector2> convertToBezier(const Vector2& p1, const Vector2 p2,
	                                                      EaseType ease)
	{
		Vector2 ctrlPoint = { 0, midpoint(p1.y, p2.y) };
		switch (ease)
		{
		case EaseType::Linear:
			ctrlPoint.x = midpoint(p1.x, p2.x);
			break;
		case EaseType::EaseIn:
			ctrlPoint.x = p1.x;
			break;
		case EaseType::EaseOut:
			ctrlPoint.x = p2.x;
			break;
		default:
			throw std::runtime_error("Can't convert to specified EaseType");
		}
		return { p1, ctrlPoint, p2 };
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