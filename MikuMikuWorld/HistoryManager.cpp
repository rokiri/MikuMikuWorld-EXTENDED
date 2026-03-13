#include "HistoryManager.h"

namespace MikuMikuWorld
{
	HistoryManager::HistoryManager() : stackIndex{ 0 }, historyStack{}
	{
		historyStack.push_back(History{ "Initial history", Score() });
	}

	const Score& HistoryManager::peekCurrent() const { return historyStack.at(stackIndex).score; }

	Score HistoryManager::undo()
	{
		stackIndex--;
		return historyStack.at(stackIndex).score;
	}

	Score HistoryManager::redo()
	{
		stackIndex++;
		return historyStack.at(stackIndex).score;
	}

	void HistoryManager::pushHistory(const std::string& description, const Score& score)
	{
		if (historyStack.size() != stackIndex + 1)
			historyStack.erase(historyStack.begin() + stackIndex + 1, historyStack.end());
		historyStack.push_back(History{ description, score });
		stackIndex = historyStack.size() - 1;
	}

	void HistoryManager::pushHistory(const History& history)
	{
		if (historyStack.size() != stackIndex + 1)
			historyStack.erase(historyStack.begin() + stackIndex + 1, historyStack.end());
		historyStack.push_back(history);
		stackIndex = historyStack.size() - 1;
	}

	void HistoryManager::clear()
	{
		historyStack.clear();
		historyStack.push_back(History{ "Initial history", Score() });
	}

	void HistoryManager::clear(const Score& score)
	{
		historyStack.clear();
		historyStack.push_back(History{ "Initial history", score });
	}

	bool HistoryManager::hasUndo() const { return stackIndex > 0; }

	bool HistoryManager::hasRedo() const { return stackIndex < (historyStack.size() - 1); }

	int HistoryManager::undoCount() const { return stackIndex; }

	int HistoryManager::redoCount() const { return historyStack.size() - stackIndex - 1; }

	std::tuple<HistoryManager::iterator, HistoryManager::iterator, HistoryManager::iterator>
	HistoryManager::getHistories() const
	{
		return std::make_tuple(historyStack.crbegin(), historyStack.crend() - stackIndex - 1,
		                       historyStack.crend());
	}
}