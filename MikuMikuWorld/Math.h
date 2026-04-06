#pragma once
#include "ImGui/imgui.h"
#include "NoteTypes.h"
#include <functional>
#include <cmath>
#include <cfenv>

namespace MikuMikuWorld
{
	struct Vector2
	{
		float x;
		float y;

		Vector2(float _x, float _y) : x{ _x }, y{ _y } {}

		Vector2() : x{ 0 }, y{ 0 } {}

		Vector2 operator+(const Vector2& v) { return Vector2(x + v.x, y + v.y); }

		Vector2 operator-(const Vector2& v) { return Vector2(x - v.x, y - v.y); }

		Vector2 operator*(const Vector2& v) { return Vector2(x * v.x, y * v.y); }

		inline Vector2(const ImVec2& vec) : x(vec.x), y(vec.y) {}
		inline operator ImVec2() const { return { x, y }; }
	};

	inline Vector2 floor(const Vector2& vec2)
	{
		return Vector2(std::floor(vec2.x), std::floor(vec2.y));
	}

	inline Vector2 round(const Vector2& vec2)
	{
		return Vector2(std::round(vec2.x), std::round(vec2.y));
	}

	inline Vector2 ceil(const Vector2& vec2)
	{
		return Vector2(std::ceil(vec2.x), std::ceil(vec2.y));
	}

	struct Color
	{
	  public:
		float r, g, b, a;

		constexpr Color() : Color(0, 0, 0, 1) {}

		constexpr Color(ImU32 col)
		    : r{ ((col >> IM_COL32_R_SHIFT) & 0xFF) / 255.f },
		      g{ ((col >> IM_COL32_G_SHIFT) & 0xFF) / 255.f },
		      b{ ((col >> IM_COL32_B_SHIFT) & 0xFF) / 255.f },
		      a{ ((col >> IM_COL32_A_SHIFT) & 0xFF) / 255.f }
		{
		}

		constexpr Color(float _r, float _g, float _b, float _a = 1.0f) noexcept
		    : r{ _r }, g{ _g }, b{ _b }, a{ _a }
		{
		}

		inline bool operator==(const Color& c)
		{
			return r == c.r && g == c.g && b == c.b && a == c.a;
		}
		inline bool operator!=(const Color& c) { return !(*this == c); }
		inline Color operator*(const Color& c) const
		{
			return Color{ r * c.r, g * c.g, b * c.b, a * c.a };
		}

		inline Color scaleAlpha(float val) const { return { r, g, b, a * val }; }

		static inline int rgbaToInt(int r, int g, int b, int a)
		{
			return r << 24 | g << 16 | b << 8 | a;
		}
		static inline int abgrToInt(int a, int b, int g, int r)
		{
			return a << 24 | b << 16 | g << 8 | r;
		}
		inline ImU32 toImU32() const { return ImGui::ColorConvertFloat4ToU32({ r, g, b, a }); }
		inline ImVec4 toImVec4() const { return ImVec4{ r, g, b, a }; }
		static inline Color fromImVec4(const ImVec4& col)
		{
			return Color{ col.x, col.y, col.z, col.w };
		}

		static inline Color lerp(const Color& start, const Color& end, float ratio)
		{
			return Color{ start.r + (end.r - start.r) * ratio, start.g + (end.g - start.g) * ratio,
				          start.b + (end.b - start.b) * ratio,
				          start.a + (end.a - start.a) * ratio };
		}
	};

	constexpr uint32_t roundUpToPowerOfTwo(uint32_t v)
	{
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	}

	template <typename FloatType = double> static auto roundOff(double value, int precision = 7)
	{
		FloatType dvalue = value;
		double digits = std::pow(10, precision);
		return std::round(dvalue * digits) / digits;
	}

	template <typename FloatType = double> static FloatType roundUnderHalf(FloatType x)
	{
		constexpr FloatType half(0.5), zero(0.0);
		return std::trunc(std::nextafter(x + std::copysign(half, x), zero));
	}

	template <typename FloatType, typename IntType>
	static FloatType roundToStep(FloatType value, IntType step)
	{
		return roundUnderHalf(value * step) / step;
	}

	template <typename FloatType>
	static bool isClose(FloatType val, FloatType tgr,
	                    FloatType epsilon = std::numeric_limits<FloatType>::epsilon())
	{
		if (val == tgr)
			return true;
		FloatType tolerance = epsilon * std::max(std::fabs(val), std::fabs(tgr));
		return std::fabs(val - tgr) <= std::max(epsilon, tolerance);
	}

	template <typename FloatType>
	static bool isDivisibleBy(FloatType num, FloatType val,
	                          FloatType epsilon = std::numeric_limits<FloatType>::epsilon())
	{
		constexpr FloatType zero = 0;
		auto f = std::fmod(std::abs(num), val);
		return isClose(f, zero) || isClose(val - f, zero);
	}

	template <typename IntType>
	static std::pair<IntType, IntType> integerDivide(IntType x, IntType y)
	{
		// Due to div_t member order not defined by the standard
		// We need to access the member directly to ensure the correct order
		auto divResult = std::div(x, y);
		return std::make_pair(divResult.quot, divResult.rem);
	}

	float lerp(float start, float end, float ratio);
	float unlerp(float start, float end, float value, float fallback = 0);
	double lerpD(double start, double end, double ratio);
	double unlerpD(double start, double end, double value, double fallback = 0);
	float easeIn(float start, float end, float ratio);
	float easeOut(float start, float end, float ratio);
	float easeInOut(float start, float end, float ratio);
	float easeOutIn(float start, float end, float ratio);
	float easeNone(float start, float end, float ratio);
	float midpoint(float x1, float x2);
	bool isWithinRange(float x, float left, float right);

	using EaseFunction = float (*)(float, float, float);
	EaseFunction getEaseFunction(EaseType ease);

	uint32_t gcf(uint32_t a, uint32_t b);
}