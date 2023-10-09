#include "db/system.h"

#include <cassert>

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{

  if(graphquery::database::Initialise(argc, argv) == graphquery::database::EStatus::invalid)
    return EXIT_FAILURE;
  
  graphquery::database::_interface->Render();

  return EXIT_SUCCESS;
}