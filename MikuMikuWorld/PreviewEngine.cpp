#include "PreviewEngine.h"
#include "Constants.h"
#include "Tempo.h"
#include <algorithm> // std::stable_sort のため
#include <vector>

namespace MikuMikuWorld
{
	SpriteTransform::SpriteTransform(float v[64]) : xx(v), xy(nullptr), yx(nullptr), yy(v + 48) 
	{
		DirectX::XMMATRIX tmp(v + 16);
		if (!DirectX::XMMatrixIsNull(tmp))
			xy = std::make_unique<DirectX::XMMATRIX>(tmp);
		tmp = DirectX::XMMATRIX{v + 32};
		if (!DirectX::XMMatrixIsNull(tmp))
			yx = std::make_unique<DirectX::XMMATRIX>(tmp);
	}

	std::array<DirectX::XMFLOAT4, 4> SpriteTransform::apply(const std::array<DirectX::XMFLOAT4, 4> &vPos) const
	{
		DirectX::XMVECTOR x = DirectX::XMVectorSet(vPos[0].x, vPos[1].x, vPos[2].x, vPos[3].x);
		DirectX::XMVECTOR y = DirectX::XMVectorSet(vPos[0].y, vPos[1].y, vPos[2].y, vPos[3].y);
		DirectX::XMVECTOR
			tx = !xy ? DirectX::XMVector4Transform(x, xx) : DirectX::XMVectorAdd(DirectX::XMVector4Transform(x, xx), DirectX::XMVector4Transform(x, *xy)),
			ty = !yx ? DirectX::XMVector4Transform(y, yy) : DirectX::XMVectorAdd(DirectX::XMVector4Transform(y, *yx), DirectX::XMVector4Transform(y, yy));
		return {{
			{ DirectX::XMVectorGetX(tx), DirectX::XMVectorGetX(ty), vPos[0].z, vPos[0].z },
			{ DirectX::XMVectorGetY(tx), DirectX::XMVectorGetY(ty), vPos[1].z, vPos[1].z },
			{ DirectX::XMVectorGetZ(tx), DirectX::XMVectorGetZ(ty), vPos[2].z, vPos[2].z },
			{ DirectX::XMVectorGetW(tx), DirectX::XMVectorGetW(ty), vPos[3].z, vPos[3].z }
		}};
	}
}

namespace MikuMikuWorld::Engine
{
	// ハイスピード対応の心臓部（レイヤー対応・NextRUSH+互換版）
	double accumulateScaledDuration(int tick, int beatTicks, const std::vector<Tempo>& tempos, const std::unordered_map<id_t, HiSpeedChange>& hiSpeeds, int layer)
	{
		// 1. unordered_mapからvectorに変換し、指定されたlayerに一致するハイスピードのみを抽出する
		std::vector<HiSpeedChange> hsList;
		hsList.reserve(hiSpeeds.size());
		for (const auto& [id, hs] : hiSpeeds)
		{
			if (hs.layer == layer)
				hsList.push_back(hs);
		}

		// そのレイヤーにハイスピード変化が一つもなければ、BPMのみの通常計算を返す
		if (hsList.empty())
			return accumulateDuration(tick, beatTicks, tempos);

		std::stable_sort(hsList.begin(), hsList.end(), [](const HiSpeedChange& a, const HiSpeedChange& b) {
			return a.tick < b.tick;
		});

		// 2. 目標Tickまでの「境界（BPM変更点 と HS変更点）」をすべてリストアップ
		std::vector<int> boundaries;
		boundaries.push_back(0);
		for (const auto& tempo : tempos) {
			if (tempo.tick <= tick) boundaries.push_back(tempo.tick);
		}
		for (const auto& hs : hsList) {
			if (hs.tick <= tick) boundaries.push_back(hs.tick);
		}
		boundaries.push_back(tick);

		std::stable_sort(boundaries.begin(), boundaries.end());
		boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

		double scaledDuration = 0.0;
		int currentTick = 0;
		double currentTime = 0.0; // 物理時間(秒)

		int hsIdx = -1; // 現在適用されているHSのインデックス

		// 指定TickでのBPMを取得するヘルパー関数
		auto getBpmAt = [&](int t) {
			double bpm = 120.0;
			for (auto it = tempos.rbegin(); it != tempos.rend(); ++it) {
				if (it->tick <= t) { bpm = it->bpm; break; }
			}
			return bpm;
		};

		// 指定Tickの物理時間(秒)を取得するヘルパー関数
		auto getTimeAt = [&](int t) {
			return accumulateDuration(t, beatTicks, tempos);
		};

		// 3. 境界から境界へ、区間ごとに積分計算を進める
		for (int nextTick : boundaries)
		{
			// NextRUSH+移植: Skip（ワープ）の処理
			// currentTick に到達した瞬間に、そのTickにあるHSのSkip値を加算する
			while (hsIdx + 1 < (int)hsList.size() && hsList[hsIdx + 1].tick == currentTick)
			{
				hsIdx++;
				if (hsList[hsIdx].skips != 0.0f)
				{
					double bpm = getBpmAt(hsList[hsIdx].tick);
					// Skipの単位(Beat)を時間(秒)に変換して直接加算（ワープ）
					scaledDuration += hsList[hsIdx].skips * (60.0 / bpm);
				}
			}

			if (nextTick == currentTick) continue;

			double nextTime = getTimeAt(nextTick);
			double deltaTime = nextTime - currentTime;
			double avgSpeed = 1.0;

			if (hsIdx >= 0)
			{
				const auto& currentHs = hsList[hsIdx];
				
				// NextRUSH+移植: Linear（直線補間）の処理
				// 次のHSが存在し、かつEaseがLinearの場合のみ台形積分を行う
				if (currentHs.ease == HiSpeedEaseType::Linear && hsIdx + 1 < (int)hsList.size())
				{
					double currentHsTime = getTimeAt(currentHs.tick);
					double nextHsTime = getTimeAt(hsList[hsIdx + 1].tick);
					double timeDiff = nextHsTime - currentHsTime;

					if (timeDiff > 1e-6)
					{
						// 物理時間(秒)ベースで、現在の速度と次の速度を線形補間して中間速度を算出
						double speedAtCurrent = currentHs.speed + (hsList[hsIdx + 1].speed - currentHs.speed) * ((currentTime - currentHsTime) / timeDiff);
						double speedAtNext = currentHs.speed + (hsList[hsIdx + 1].speed - currentHs.speed) * ((nextTime - currentHsTime) / timeDiff);
						
						// 台形の面積の公式（(上底＋下底)÷2）により、この区間の平均速度を求める
						avgSpeed = (speedAtCurrent + speedAtNext) / 2.0;
					}
					else
					{
						avgSpeed = currentHs.speed;
					}
				}
				else
				{
					// None（一定速度）、または次のHSが無い場合は開始時の速度を維持
					avgSpeed = currentHs.speed;
				}
			}

			// 仮想時間(Scaled Time)を進める：(物理経過時間) * (その区間の平均速度)
			scaledDuration += deltaTime * avgSpeed;

			currentTick = nextTick;
			currentTime = nextTime;
		}

		return scaledDuration;
	}

	Range getNoteVisualTime(Note const& note, Score const& score, float noteSpeed)
	{
		//  事前計算時に、そのノーツが所属するレイヤーのハイスピードを適用する
		double targetTime = accumulateScaledDuration(note.tick, TICKS_PER_BEAT, score.tempoChanges, score.hiSpeedChanges, note.layer);
		return {targetTime - getNoteDuration(noteSpeed), targetTime};
	}

	std::array<DirectX::XMFLOAT4, 4> quadvPos(float left, float right, float top, float bottom)
	{
		return {{
			{ right, bottom, 0.0f, 1.0f },
			{ right,    top, 0.0f, 1.0f },
			{  left,    top, 0.0f, 1.0f },
			{  left, bottom, 0.0f, 1.0f }
		}};
	}

	std::array<DirectX::XMFLOAT4, 4> perspectiveQuadvPos(float left, float right, float top, float bottom)
	{
		float x1 = right * top,    y1 = top,
				x2 = right * bottom, y2 = bottom,
				x3 = left * bottom,  y3 = bottom,
				x4 = left * top,     y4 = top;
		return {{
			{ x1, y1, 0.0f, 1.0f },
			{ x2, y2, 0.0f, 1.0f },
			{ x3, y3, 0.0f, 1.0f },
			{ x4, y4, 0.0f, 1.0f }
		}};
	}

	std::array<DirectX::XMFLOAT4, 4> perspectiveQuadvPos(float leftStart, float leftStop, float rightStart, float rightStop, float top, float bottom)
	{
		float 
			x1 = rightStart * top,   y1 = top,
			x2 = rightStop * bottom, y2 = bottom,
			x3 = leftStop * bottom,  y3 = bottom,
			x4 = leftStart * top,    y4 = top;
		return {{
			{ x1, y1, 0.0f, 1.0f },
			{ x2, y2, 0.0f, 1.0f },
			{ x3, y3, 0.0f, 1.0f },
			{ x4, y4, 0.0f, 1.0f }
		}};
	}

	std::array<DirectX::XMFLOAT4, 4> quadUV(const Sprite& sprite, const Texture &texture)
	{
		float left = sprite.getX() / texture.getWidth();
		float right = (sprite.getX() + sprite.getWidth()) / texture.getWidth();
		float top = sprite.getY() / texture.getHeight();
		float bottom = (sprite.getY() + sprite.getHeight()) / texture.getHeight();
		return quadvPos(left, right, top, bottom);
	}
}