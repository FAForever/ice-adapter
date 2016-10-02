#include <gio/gnetworking.h>

#include "IceAdapter.h"

int main(int argc, char *argv[])
{
  g_networking_init();

  IceAdapter a("1234",
               "http://localhhost:5000/");
  a.run();

  return 0;
}
