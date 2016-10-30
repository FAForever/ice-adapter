#include <iostream>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
//#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <giomm.h>
#include <glibmm.h>

#include "IceAdapter.h"
#include "IceAdapterOptions.h"

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

int main(int argc, char *argv[])
{
  try
  {
    Gio::init();

    boost::log::add_console_log(
      std::cout,
      boost::log::keywords::format = boost::log::expressions::stream
                                     << boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
                                     << ": <" << boost::log::trivial::severity
                                     << "> " << boost::log::expressions::smessage,
      boost::log::keywords::auto_flush = true
    );
    boost::log::add_common_attributes();

    boost::log::core::get()->set_filter (
        boost::log::trivial::severity >= boost::log::trivial::trace
        );

    auto options = IceAdapterOptions::init(argc, argv);

    auto loop = Glib::MainLoop::create();

    IceAdapter a(options,
                 loop);
    loop->run();
  }
  catch (const Gio::Error& e)
  {
    BOOST_LOG_TRIVIAL(error) << "Glib error: " << e.what();
    return 1;
  }
  catch (const std::exception& e)
  {
    BOOST_LOG_TRIVIAL(error) << "error: " << e.what();
    return 1;
  }
  catch (...)
  {
    BOOST_LOG_TRIVIAL(error) << "unknown error occured";
    return 1;
  }

  return 0;
}
