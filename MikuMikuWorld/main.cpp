#include "Application.h"
#include "IO.h"
#include "PlatformIO.h"
#include <iostream>

namespace mmw = MikuMikuWorld;

int main()
{
	std::vector<std::string> args = IO::getCommandLineArgs();
	if (args.empty())
	{
		IO::messageBox(APP_NAME, "Something went wrong!\nCommandline arguments not found...",
		               IO::MessageBoxButtons::Ok, IO::MessageBoxIcon::Error);
		return 1;
	}

	mmw::Application app;
#ifndef DEBUG
	try
	{
#endif
		mmw::Result result = app.initialize(args[0]);
		if (!result.isOk())
			throw std::runtime_error(result.getMessage());
		 for (size_t i = 1; i < args.size(); ++i)
			app.appendOpenFile(IO::stringToPath(args[i]));
		app.run();
#ifndef DEBUG
	}
	catch (const std::exception& ex)
	{
		std::string msg =
		    std::string(
		        "An unhandled exception has occurred and the application will now close.\n\n")
		        .append(ex.what())
		        .append("\n\nApplication Version: ")
		        .append(mmw::Application::getAppVersion());

		IO::messageBox(APP_NAME, msg, IO::MessageBoxButtons::Ok, IO::MessageBoxIcon::Error);
	}
#endif

	app.dispose();
	return 0;
}