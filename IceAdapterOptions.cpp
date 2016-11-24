#include "IceAdapterOptions.h"

#include <cstdlib>

#include <boost/program_options.hpp>

#include "logging.h"

namespace po = boost::program_options;

IceAdapterOptions::IceAdapterOptions()
{
}

IceAdapterOptions IceAdapterOptions::init(int argc, char *argv[])
{
  IceAdapterOptions result;

  try
  {
      po::options_description desc("faf-ice-adapter usage");
      desc.add_options()
        ("help",           "produce help message")
        ("id",           po::value<int>(&result.localPlayerId)->required(),                            "set the ID of the local player")
        ("login",        po::value<std::string>(&result.localPlayerLogin)->required(),                 "set the login of the local player, e.g. \"Rhiza\"")
        ("rpc-port",     po::value<int>(&result.rpcPort)->default_value(7236),                         "set the port of internal JSON-RPC server")
        ("ice-port-min", po::value<int>(&result.iceLocalPortMin)->default_value(-1),                   "start of port range to use for ICE local host candidates")
        ("ice-port-max", po::value<int>(&result.iceLocalPortMax)->default_value(-1),                   "end of port range to use for ICE local host candidates")
        ("upnp",         po::value<bool>(&result.useUpnp)->default_value(true),                        "use UPNP for NAT router port configuration")
        ("gpgnet-port",  po::value<int>(&result.gpgNetPort)->default_value(7237),                      "set the port of internal GPGNet server")
        ("lobby-port",   po::value<int>(&result.gameUdpPort)->default_value(7238),                     "set the port the game lobby should use for incoming UDP packets from the PeerRelay")
        ("stun-host",    po::value<std::string>(&result.stunHost)->default_value("dev.faforever.com"), "set the STUN hostname")
        ("turn-host",    po::value<std::string>(&result.turnHost)->default_value("dev.faforever.com"), "set the TURN hostname")
        ("turn-user",    po::value<std::string>(&result.turnUser)->default_value(""),                  "set the TURN username")
        ("turn-pass",    po::value<std::string>(&result.turnPass)->default_value(""),                  "set the TURN password")
        ("log-file",     po::value<std::string>(&result.logFile)->default_value(""),                   "set a verbose log file")
      ;

      try
      {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            FAF_LOG_ERROR << desc;
            std::exit(1);
        }

        po::notify(vm);
      }
      catch(std::exception& e)
      {
        FAF_LOG_ERROR << "Error: " << e.what() << "\n" << desc;
        std::exit(1);

      }
      catch(...)
      {
        FAF_LOG_ERROR << "Unknown error!" << "\n" << desc;
        std::exit(1);
      }
  }
  catch(std::exception& e)
  {
      FAF_LOG_ERROR << "Error: " << e.what();
      std::exit(1);
  }
  catch(...)
  {
      FAF_LOG_ERROR << "Unknown error!";
      std::exit(1);
  }
  return result;
}
