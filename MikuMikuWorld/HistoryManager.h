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
	};

	class HistoryManager
	{
	  private:
		size_t stackIndex;
		std::vector<History> historyStack;

		using iterator = std::vector<History>::const_reverse_iterator;

	  public:
		HistoryManager();
		const Score& peekCurrent() const;
		Score undo();
		Score redo();

		int undoCount() const;
		int redoCount() const;
		std::tuple<iterator, iterator, iterator> getHistories() const;

		void pushHistory(const History& history);
		void pushHistory(const std::string& description, const Score& score);
		void clear();
		void clear(const Score& score);
		bool hasUndo() const;
		bool hasRedo() const;
	};
}