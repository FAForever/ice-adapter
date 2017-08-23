#include "IceAdapterOptions.h"

#include <cstdlib>
#include <iostream>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace faf
{

IceAdapterOptions::IceAdapterOptions()
{
}

IceAdapterOptions IceAdapterOptions::init(int argc, char *argv[])
{
  IceAdapterOptions result;

  try
  {
      po::options_description desc(std::string("faf-ice-adapter ") + FAF_VERSION_STRING + " usage");
      desc.add_options()
        ("help",           "produce help message")
        ("id",           po::value<int>(&result.localPlayerId)->required(),                            "set the ID of the local player")
        ("login",        po::value<std::string>(&result.localPlayerLogin)->required(),                 "set the login of the local player, e.g. \"Rhiza\"")
        ("rpc-port",     po::value<int>(&result.rpcPort)->default_value(7236),                         "set the port of internal JSON-RPC server")
        ("gpgnet-port",  po::value<int>(&result.gpgNetPort)->default_value(7237),                      "set the port of internal GPGNet server")
        ("lobby-port",   po::value<int>(&result.gameUdpPort)->default_value(7238),                     "set the port the game lobby should use for incoming UDP packets from the PeerRelay")
        ("log-file",     po::value<std::string>(&result.logFile)->default_value(""),                   "set a verbose log file")
        ("log-level",    po::value<std::string>(&result.logFile)->default_value("debug"),              "set the logging vebosity level: error, warn, info, debug or trace")
      ;

      try
      {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cerr << desc;
            std::exit(1);
        }

        po::notify(vm);
      }
      catch(std::exception& e)
      {
        std::cerr << "Error: " << e.what() << "\n" << desc;
        std::exit(1);

      }
      catch(...)
      {
        std::cerr << "Unknown error!" << "\n" << desc;
        std::exit(1);
      }
  }
  catch(std::exception& e)
  {
      std::cerr << "Error: " << e.what();
      std::exit(1);
  }
  catch(...)
  {
      std::cerr << "Unknown error!";
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
