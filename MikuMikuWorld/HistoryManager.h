#pragma once
#include <stack>
#include <map>
#include <unordered_map>
#include <string>
#include "Score.h"

namespace MikuMikuWorld
{
	struct History
	{
		std::string description;
		Score score;
		ScoreMetadata metadata;
	};

	class HistoryManager
	{
	  private:
		size_t stackIndex;
		std::vector<History> historyStack;

		using iterator = std::vector<History>::const_reverse_iterator;

	  public:
		HistoryManager();
		const History& peekCurrent() const;
		const History& undo();
		const History& redo();

		int undoCount() const;
		int redoCount() const;
		std::tuple<iterator, iterator, iterator> getHistories() const;

		void pushHistory(const History& history);
		void pushHistory(std::string_view description, const Score& score,
		                 const ScoreMetadata& metadata);
		void clear();
		void clear(const Score& score, const ScoreMetadata& metadata);
		bool hasUndo() const;
		bool hasRedo() const;
	};
}