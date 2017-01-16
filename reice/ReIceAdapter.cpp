#include "ReIceAdapter.h"

#include <cstdint>

#include <re.h>

namespace faf {

IceAdapter::IceAdapter()
{

}

}


int main(int argc, char *argv[])
{
  auto err = libre_init();
  if (err)
  {
    return 1;
  }

  libre_close();
  return 0;
}
