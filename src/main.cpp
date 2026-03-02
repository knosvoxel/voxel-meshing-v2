#include "application.h"

//#ifndef _DEBUG
//
//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
//
//#endif

int main() {
	Application app;

	try {
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}