#include "IceAdapterOptions.h"

#include <cstdlib>

#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>

namespace po = boost::program_options;

IceAdapterOptions::IceAdapterOptions():
  Glib::OptionGroup("main",
                    "")
{
  {
    Glib::OptionEntry entry;
    entry.set_long_name("login");
    entry.set_short_name('l');
    entry.set_description("Login of the local player, e.g. \"Rhiza\"");
    add_entry(entry, localPlayerLogin);
  }


  {
    Glib::OptionEntry entry;
    entry.set_long_name("id");
    entry.set_short_name('i');
    entry.set_description("ID of the local player");
    localPlayerId = -1;
    add_entry(entry, localPlayerId);
  }

  {
    Glib::OptionEntry entry;
    entry.set_long_name("rpc-port");
    entry.set_short_name('p');
    entry.set_description("Port of internal JSON-RPC server, default: 7236");
    rpcPort = 7236;
    add_entry(entry, rpcPort);
  }

  {
    Glib::OptionEntry entry;
    entry.set_long_name("gpgnet-port");
    entry.set_short_name('g');
    entry.set_description("Port of internal GPGNet server, default: 7237. May be 0 for dynamic port.");
    gpgNetPort = 7237;
    add_entry(entry, gpgNetPort);
  }

  {
    Glib::OptionEntry entry;
    entry.set_long_name("lobby-port");
    entry.set_short_name('a');
    entry.set_description("Port the game lobby should use for incoming UDP packets from the PeerRelay, default: 7238");
    gameUdpPort = 7238;
    add_entry(entry, gameUdpPort);
  }

  {
    Glib::OptionEntry entry;
    entry.set_long_name("stun-host");
    entry.set_short_name('s');
    entry.set_description("STUN-host, default: dev.faforever.com");
    stunHost = "dev.faforever.com";
    add_entry(entry, stunHost);
  }

  {
    Glib::OptionEntry entry;
    entry.set_long_name("turn-host");
    entry.set_short_name('t');
    entry.set_description("TURN-host, default: dev.faforever.com");
    turnHost = "dev.faforever.com";
    add_entry(entry, turnHost);
  }

  {
    Glib::OptionEntry entry;
    entry.set_long_name("turn-user");
    entry.set_short_name('u');
    entry.set_description("TURN-user, default: \"\"");
    turnUser = "";
    add_entry(entry, turnUser);
  }

  {
    Glib::OptionEntry entry;
    entry.set_long_name("turn-pass");
    entry.set_short_name('x');
    entry.set_description("TURN-password, default: \"\"");
    turnPass = "";
    add_entry(entry, turnPass);
  }
  relayUdpPortStart = 7240;
}

IceAdapterOptionsPtr IceAdapterOptions::init(int argc, char *argv[])
{
  auto result = IceAdapterOptionsPtr(new IceAdapterOptions);
  Glib::OptionContext option_context("- FAF ICE Adapter");
  option_context.set_summary("see https://github.com/FAForever/ice-adapter");
  option_context.set_main_group(*result);
  try
  {
    if (!option_context.parse(argc, argv))
    {
      BOOST_LOG_TRIVIAL(error) << "Error parsing commandline.\n" << option_context.get_help();
      std::exit(1);
    }
  }
  catch(const Glib::Error & e)
  {
    BOOST_LOG_TRIVIAL(error) << "Error parsing commandline.\n" << option_context.get_help();
    std::exit(1);
  }

  if (result->localPlayerId == -1)
  {
    BOOST_LOG_TRIVIAL(error) << "Missing parameter 'id'.\n" << option_context.get_help();
    std::exit(1);
  }

  if (result->localPlayerLogin.empty())
  {
    BOOST_LOG_TRIVIAL(error) << "Missing parameter 'login'.\n" << option_context.get_help();
    std::exit(1);
  }

  return result;
}
