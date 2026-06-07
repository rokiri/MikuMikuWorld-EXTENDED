#include "PreviewEngine.h"
#include "Constants.h"
#include "Tempo.h"
#include <algorithm>
#include <vector>

namespace MikuMikuWorld
{
	SpriteTransform::SpriteTransform(float v[64]) : xx(v), xy(nullptr), yx(nullptr), yy(v + 48)
	{
		DirectX::XMMATRIX tmp(v + 16);
		if (!DirectX::XMMatrixIsNull(tmp))
			xy = std::make_unique<DirectX::XMMATRIX>(tmp);
		tmp = DirectX::XMMATRIX{ v + 32 };
		if (!DirectX::XMMatrixIsNull(tmp))
			yx = std::make_unique<DirectX::XMMATRIX>(tmp);
	}

	std::array<DirectX::XMFLOAT4, 4>
	SpriteTransform::apply(const std::array<DirectX::XMFLOAT4, 4>& vPos) const
	{
		DirectX::XMVECTOR x = DirectX::XMVectorSet(vPos[0].x, vPos[1].x, vPos[2].x, vPos[3].x);
		DirectX::XMVECTOR y = DirectX::XMVectorSet(vPos[0].y, vPos[1].y, vPos[2].y, vPos[3].y);
		DirectX::XMVECTOR tx = !xy ? DirectX::XMVector4Transform(x, xx)
		                           : DirectX::XMVectorAdd(DirectX::XMVector4Transform(x, xx),
		                                                  DirectX::XMVector4Transform(x, *xy)),
		                  ty = !yx ? DirectX::XMVector4Transform(y, yy)
		                           : DirectX::XMVectorAdd(DirectX::XMVector4Transform(y, *yx),
		                                                  DirectX::XMVector4Transform(y, yy));
		return { { { DirectX::XMVectorGetX(tx), DirectX::XMVectorGetX(ty), vPos[0].z, vPos[0].z },
			       { DirectX::XMVectorGetY(tx), DirectX::XMVectorGetY(ty), vPos[1].z, vPos[1].z },
			       { DirectX::XMVectorGetZ(tx), DirectX::XMVectorGetZ(ty), vPos[2].z, vPos[2].z },
			       { DirectX::XMVectorGetW(tx), DirectX::XMVectorGetW(ty), vPos[3].z,
			         vPos[3].z } } };
	}
}

namespace MikuMikuWorld::Engine
{
	double accumulateScaledDuration(int tick, int beatTicks, const std::vector<Tempo>& tempos,
	                                const HiSpeedCollection& hiSpeeds, int layer)
	{
		if (hiSpeeds.empty())
			return accumulateDuration(tick, TempoCollection{});

		std::vector<int> boundaries;
		boundaries.push_back(0);
		for (const auto& tempo : tempos)
			if (tempo.tick <= tick)
				boundaries.push_back(tempo.tick);
		for (const auto& [t, hs] : hiSpeeds)
			if (t <= tick)
				boundaries.push_back(t);
		boundaries.push_back(tick);

		std::stable_sort(boundaries.begin(), boundaries.end());
		boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

		double scaledDuration = 0.0;
		int currentTick = 0;
		double currentTime = 0.0;

		auto getTimeAt = [&](int t) -> double
		{
			// Build a TempoCollection from vector for compatibility
			TempoCollection tc;
			for (const auto& tempo : tempos)
				tc[tempo.tick] = tempo;
			return accumulateDuration(t, tc);
		};

		for (int nextTick : boundaries)
		{
			// Handle skips at currentTick
			auto hsIt = hiSpeeds.find(currentTick);
			if (hsIt != hiSpeeds.end() && hsIt->second.skips != 0.0f)
			{
				double bpm = 120.0;
				for (auto it = tempos.rbegin(); it != tempos.rend(); ++it)
					if (it->tick <= currentTick)
					{
						bpm = it->quarterPerMinute;
						break;
					}
				scaledDuration += hsIt->second.skips * (60.0 / bpm);
			}

			if (nextTick == currentTick)
				continue;

			double nextTime = getTimeAt(nextTick);
			double deltaTime = nextTime - currentTime;
			double avgSpeed = 1.0;

			auto curHsIt = hiSpeeds.upper_bound(currentTick);
			if (curHsIt != hiSpeeds.begin())
			{
				--curHsIt;
				const auto& currentHs = curHsIt->second;
				auto nextHsIt = std::next(curHsIt);

				if (currentHs.ease == HiSpeedEaseType::Linear && nextHsIt != hiSpeeds.end())
				{
					double currentHsTime = getTimeAt(curHsIt->first);
					double nextHsTime = getTimeAt(nextHsIt->first);
					double timeDiff = nextHsTime - currentHsTime;
					if (timeDiff > 1e-6)
					{
						double speedAtCurrent =
						    currentHs.speed + (nextHsIt->second.speed - currentHs.speed) *
						                          ((currentTime - currentHsTime) / timeDiff);
						double speedAtNext =
						    currentHs.speed + (nextHsIt->second.speed - currentHs.speed) *
						                          ((nextTime - currentHsTime) / timeDiff);
						avgSpeed = (speedAtCurrent + speedAtNext) / 2.0;
					}
					else
						avgSpeed = currentHs.speed;
				}
				else
					avgSpeed = currentHs.speed;
			}

			scaledDuration += deltaTime * avgSpeed;
			currentTick = nextTick;
			currentTime = nextTime;
		}

		return scaledDuration;
	}

	Range getNoteVisualTime(Note const& note, Score const& score, float noteSpeed)
	{
		double targetTime = accumulateScaledDuration(note.tick, score);
		return Range{ targetTime - getNoteDuration(noteSpeed), targetTime };
	}

	std::array<DirectX::XMFLOAT4, 4> quadvPos(float left, float right, float top, float bottom)
	{
		return { { { right, bottom, 0.0f, 1.0f },
			       { right, top, 0.0f, 1.0f },
			       { left, top, 0.0f, 1.0f },
			       { left, bottom, 0.0f, 1.0f } } };
	}

	std::array<DirectX::XMFLOAT4, 4> perspectiveQuadvPos(float left, float right, float top,
	                                                     float bottom)
	{
		float x1 = right * top, y1 = top, x2 = right * bottom, y2 = bottom, x3 = left * bottom,
		      y3 = bottom, x4 = left * top, y4 = top;
		return { { { x1, y1, 0.0f, 1.0f },
			       { x2, y2, 0.0f, 1.0f },
			       { x3, y3, 0.0f, 1.0f },
			       { x4, y4, 0.0f, 1.0f } } };
	}

	std::array<DirectX::XMFLOAT4, 4> perspectiveQuadvPos(float leftStart, float leftStop,
	                                                     float rightStart, float rightStop,
	                                                     float top, float bottom)
	{
		float x1 = rightStart * top, y1 = top, x2 = rightStop * bottom, y2 = bottom,
		      x3 = leftStop * bottom, y3 = bottom, x4 = leftStart * top, y4 = top;
		return { { { x1, y1, 0.0f, 1.0f },
			       { x2, y2, 0.0f, 1.0f },
			       { x3, y3, 0.0f, 1.0f },
			       { x4, y4, 0.0f, 1.0f } } };
	}

	std::array<DirectX::XMFLOAT4, 4> quadUV(const Sprite& sprite, const Texture& texture)
	{
		float left = sprite.getX1() / texture.getWidth();
		float right = sprite.getX2() / texture.getWidth();
		float top = sprite.getY1() / texture.getHeight();
		float bottom = sprite.getY2() / texture.getHeight();
		return quadvPos(left, right, top, bottom);
	}
}