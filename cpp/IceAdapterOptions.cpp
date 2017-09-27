#include "IceAdapterOptions.h"

#include <cstdlib>
#include <iostream>

#include "cxxopts.hpp"

namespace faf
{

IceAdapterOptions::IceAdapterOptions():
  rpcPort(7236),
  gpgNetPort(0),
  gameUdpPort(7238),
  logLevel("debug")
{
}

IceAdapterOptions IceAdapterOptions::init(int argc, char *argv[])
{
  IceAdapterOptions result;

  cxxopts::Options options("faf-ice-adapter", "A P2P connection proxy for Supreme Commander: Forged Alliance using ICE");
  options.add_options()
    ("help", "Show this help message")
    ("id", "set the ID of the local player", cxxopts::value<int>(result.localPlayerId))
    ("login", "set the login of the local player, e.g. \"Rhiza\"", cxxopts::value<std::string>(result.localPlayerLogin))
    ("rpc-port", "set the port of internal JSON-RPC server", cxxopts::value<int>(result.rpcPort))
    ("gpgnet-port", "set the port of internal GPGNet server", cxxopts::value<int>(result.gpgNetPort))
    ("lobby-port", "set the port the game lobby should use for incoming UDP packets from the PeerRelay", cxxopts::value<int>(result.gameUdpPort))
    ("log-file", "log to specified file", cxxopts::value<std::string>(result.logFile))
    ("log-level", "set logging verbosity level: error, warn, info, verbose or debug", cxxopts::value<std::string>(result.logLevel))
    ;

  options.parse(argc, argv);

  if (options.count("help"))
  {
    std::cout << options.help() << std::endl;
    std::exit(0);
  }
  if (options.count("id") == 0)
  {
    std::cerr << "argument id is required" << std::endl;
    std::cout << options.help() << std::endl;
    std::exit(1);
  }
  if (options.count("login") == 0)
  {
    std::cerr << "argument login is required" << std::endl;
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
