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

namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

class ServerOptionGroup : public Glib::OptionGroup, public IceAdapterOptions
{
public:

  ServerOptionGroup() : Glib::OptionGroup("server_group", "", "")
  {
    Glib::OptionEntry entry;

    entry.set_long_name("stun-host");
    entry.set_short_name('s');
    entry.set_description("STUN-host, default: dev.faforever.com");
    add_entry(entry, stunHost);

    entry.set_long_name("turn-host");
    entry.set_short_name('t');
    entry.set_description("TURN-host, default: dev.faforever.com");
    add_entry(entry, turnHost);

    entry.set_long_name("turn-user");
    entry.set_short_name('u');
    entry.set_description("TURN-user, default: ");
    add_entry(entry, turnUser);

    entry.set_long_name("turn-pass");
    entry.set_short_name('x');
    entry.set_description("TURN-password, default: ");
    add_entry(entry, turnPass);

    entry.set_long_name("rpc-port");
    entry.set_short_name('p');
    entry.set_description("Port of internal JSON-RPC server, default: 54321");
    add_entry(entry, rpcPort);

    entry.set_long_name("gpgnet-port");
    entry.set_short_name('g');
    entry.set_description("Port of internal GPGNet server, default: 6113");
    add_entry(entry, gpgNetPort);
  }
};

int main(int argc, char *argv[])
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

  Glib::OptionContext option_context(" - FAF ICE Adapter server settings");
  ServerOptionGroup option_group;
  option_context.set_main_group(option_group);
  try
  {
    option_context.parse(argc, argv);
  }
  catch (const Glib::Error& error)
  {
    BOOST_LOG_TRIVIAL(error) << "Error parsing options: " << error.what();
    return 1;
  }

  auto loop = Glib::MainLoop::create();

  try
  {
    IceAdapter a(option_group,
                 loop);
    loop->run();
  }
  catch (const Gio::Error& error)
  {
    BOOST_LOG_TRIVIAL(error) << "Exception caught: " << error.what();
    return 1;
  }

  return 0;
}
