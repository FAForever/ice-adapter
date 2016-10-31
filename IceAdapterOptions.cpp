#include "IceAdapterOptions.h"

#include <cstdlib>

#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>

namespace po = boost::program_options;

IceAdapterOptions::IceAdapterOptions():
  relayUdpPortStart(7240)
{
}

IceAdapterOptions IceAdapterOptions::init(int argc, char *argv[])
{
  IceAdapterOptions result;

  try
  {
      po::options_description desc("faf-ice-adapter usage");
      desc.add_options()
        ("help",          "produce help message")
        ("id,i",          po::value<int>(&result.localPlayerId)->required(),                            "set the ID of the local player")
        ("login,l",       po::value<std::string>(&result.localPlayerLogin)->required(),                 "set the login of the local player, e.g. \"Rhiza\"")
        ("rpc-port,p",    po::value<int>(&result.rpcPort)->default_value(7236),                         "set the port of internal JSON-RPC server")
        ("gpgnet-port,g", po::value<int>(&result.gpgNetPort)->default_value(7237),                      "set the port of internal GPGNet server")
        ("lobby-port,o",  po::value<int>(&result.gameUdpPort)->default_value(7238),                     "set the port the game lobby should use for incoming UDP packets from the PeerRelay")
        ("stun-host,s",   po::value<std::string>(&result.stunHost)->default_value("dev.faforever.com"), "set the STUN hostname")
        ("turn-host,t",   po::value<std::string>(&result.turnHost)->default_value("dev.faforever.com"), "set the TURN hostname")
        ("turn-user,u",   po::value<std::string>(&result.turnUser)->default_value(""),                  "set the TURN username")
        ("turn-pass,x",   po::value<std::string>(&result.turnPass)->default_value(""),                  "set the TURN password")
      ;

      try
      {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            BOOST_LOG_TRIVIAL(error) << desc;
            std::exit(1);
        }

        // There must be an easy way to handle the relationship between the
        // option "help" and "host"-"port"-"config"
        // Yes, the magic is putting the po::notify after "help" option check
        po::notify(vm);
      }
      catch(std::exception& e)
      {
        BOOST_LOG_TRIVIAL(error) << "Error: " << e.what() << "\n" << desc;
        std::exit(1);

      }
      catch(...)
      {
        BOOST_LOG_TRIVIAL(error) << "Unknown error!" << "\n" << desc;
        std::exit(1);
      }
  }
  catch(std::exception& e)
  {
      BOOST_LOG_TRIVIAL(error) << "Error: " << e.what();
      std::exit(1);
  }
  catch(...)
  {
      BOOST_LOG_TRIVIAL(error) << "Unknown error!";
      std::exit(1);
  }
  return result;
}
