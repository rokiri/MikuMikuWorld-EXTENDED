#include "Application.h"
#include "IO.h"
#include <iostream>

namespace mmw = MikuMikuWorld;

int main()
{
	int argc;
	LPWSTR* args;
	args = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!args)
	{
		IO::messageBox(APP_NAME, "CommandLineToArgvW failed...", IO::MessageBoxButtons::Ok,
		               IO::MessageBoxIcon::Error);
		return 1;
	}

	mmw::Application app;
#ifndef DEBUG
	try
	{
#endif
		std::string dir = IO::File::getFilepath(IO::wideStringToMb(args[0]));
		mmw::Result result = app.initialize(dir);

		if (!result.isOk())
			throw(std::exception(result.getMessage().c_str()));

		//for (int i = 1; i < argc; ++i)
		//	app.appendOpenFile(IO::wideStringToMb(args[i]));

		// app.handlePendingOpenFiles();
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