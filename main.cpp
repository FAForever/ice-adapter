#include <giomm.h>
#include <glibmm.h>

#include "IceAdapter.h"
#include "IceAdapterOptions.h"
#include "logging.h"

#include <nice.h>

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

int main(int argc, char *argv[])
{
  try
  {
    Gio::init();

    faf::logging_init();

    nice_debug_disable(true);

    auto options = faf::IceAdapterOptions::init(argc, argv);

    if (!options.logFile.empty())
    {
      faf::logging_init_log_file(options.logFile);
    }

    auto loop = Glib::MainLoop::create();

    faf::IceAdapter a(options,
                      loop);
    loop->run();
  }
  catch (const Gio::Error& e)
  {
    FAF_LOG_ERROR << "Glib error: " << e.what();
    return 1;
  }
  catch (const std::exception& e)
  {
    FAF_LOG_ERROR << "error: " << e.what();
    return 1;
  }
  catch (...)
  {
    FAF_LOG_ERROR << "unknown error occured";
    return 1;
  }

  return 0;
}
