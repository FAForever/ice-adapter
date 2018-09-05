#include "IceAdapterOptions.h"

#include <cstdlib>
#include <iostream>

#include "cxxopts.hpp"

namespace faf
{

IceAdapterOptions::IceAdapterOptions():
  rpcPort(7236),
  gpgNetPort(0),
  gameUdpPort(0),
  logLevel("info")
{
}

IceAdapterOptions IceAdapterOptions::init(int argc, char *argv[])
{
  IceAdapterOptions result;

  cxxopts::Options options("faf-ice-adapter", "❆ faf-ice-adapter ❆\nversion " FAF_VERSION_STRING "\nA P2P connection proxy for Supreme Commander: Forged Alliance using ICE");
  options.add_options()
    ("help", "Show this help message")
    ("version", "Print the version string")
    ("id", "set the ID of the local player", cxxopts::value<int>(result.localPlayerId))
    ("login", "set the login of the local player, e.g. \"Rhiza\"", cxxopts::value<std::string>(result.localPlayerLogin))
    ("rpc-port", "set the port of internal JSON-RPC server (default: 7236)", cxxopts::value<int>(result.rpcPort))
    ("gpgnet-port", "set the port of internal GPGNet server. Set to 0 to use an automatic port. (default: 0)", cxxopts::value<int>(result.gpgNetPort))
    ("lobby-port", "set the port the game lobby should use for incoming UDP packets from the PeerRelay. Set to 0 to use an automatic port. (default: 0)", cxxopts::value<int>(result.gameUdpPort))
    ("log-directory", "log to specified directory", cxxopts::value<std::string>(result.logDirectory))
    ("log-level", "set logging verbosity level: error, warn, info, verbose or debug", cxxopts::value<std::string>(result.logLevel))
    ;

  options.parse(argc, argv);

  if (options.count("help"))
  {
    std::cout << options.help() << std::endl;
    std::exit(0);
  }
  if (options.count("version"))
  {
    std::cout << FAF_VERSION_STRING << std::endl;
    std::exit(0);
  }
  if (options.count("id") == 0)
  {
    std::cerr << "Error: argument id is required\n" << std::endl;
    std::cout << options.help() << std::endl;
    std::exit(1);
  }
  if (options.count("login") == 0)
  {
    std::cerr << "Error: argument login is required\n" << std::endl;
    std::cout << options.help() << std::endl;
    std::exit(1);
  }

  return result;
}

IceAdapterOptions IceAdapterOptions::init(int id, std::string const& login)
{
  IceAdapterOptions result;
  result.localPlayerId = id;
  result.localPlayerLogin = login;
  return result;
}

}
