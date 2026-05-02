#include "InputConfiguration.h"
#include "ScoreContext.h"
#include <iterator> // std::size

namespace MikuMikuWorld
{
	static_assert(
	    std::size(allInputBindings) == sizeof(InputConfiguration) / sizeof(MultiInputBinding),
	    "Missing or extra bindings. Make sure you don't add anything extra to InputConfiguration");

	static_assert(std::size(insertModeBindings) == size_t(InsertMode::InsertModeMax),
	              "Missing or extra bindings");
}
