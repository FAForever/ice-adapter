#include <string>
#include <iostream>

#include "PjIceGlobal.h"
#include "HttpTest.h"

int main(int argc, char *argv[])
{
  PjIceGlobal pjGlobal;
  HttpTest http(&pjGlobal);

  while (true)
  {
    pjGlobal.run();
    http.run();
  }

  return 0;
}
