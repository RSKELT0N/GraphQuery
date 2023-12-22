#include "db/system.h"

int
main([[maybe_unused]] int argc, [[maybe_unused]] char * argv[])
{
	if (graphquery::database::initialise(argc, argv) == graphquery::database::EStatus::invalid)
	{
		return EXIT_FAILURE;
	}

	graphquery::database::_interface->render();
	return EXIT_SUCCESS;
}