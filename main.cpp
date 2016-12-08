#include <giomm.h>
#include <glibmm.h>

#include "IceAdapter.h"
#include "IceAdapterOptions.h"
#include "logging.h"

#include <nice.h>

int main(int argc, char *argv[])
{
  try
  {
    Glib::add_exception_handler([]()
    {
      try
      {
        throw; // rethrow exception
      }
      catch(const Gio::Error& e)
      {
        FAF_LOG_ERROR << "Gio error: " << e.what();
      }
      catch(const Glib::Error& e)
      {
        FAF_LOG_ERROR << "Glib error: " << e.what();
      }
    });

    Gio::init();

    faf::logging_init();

    nice_debug_disable(false);

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
  catch(const Gio::Error& e)
  {
    FAF_LOG_ERROR << "Gio error: " << e.what();
  }
  catch(const Glib::Error& e)
  {
    FAF_LOG_ERROR << "Glib error: " << e.what();
  }
  catch (const std::exception& e)
  {
    FAF_LOG_ERROR << "exception: " << e.what();
    return 1;
  }
  catch (...)
  {
    FAF_LOG_ERROR << "unknown error occured";
    return 1;
  }

  return 0;
}
