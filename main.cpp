#include "IceAdapter.h"

int main(int argc, char *argv[])
{
  faf::IceAdapter iceAdapter(argc, argv);

  iceAdapter.run();
  return 0;
}
