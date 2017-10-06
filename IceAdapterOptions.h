#pragma once

#include <string>

namespace faf
{

/*! \brief Options struct
 */
class IceAdapterOptions
{
public:
  int localPlayerId;      /*!< ID of the local player */
  std::string localPlayerLogin; /*!< Login of the local player */
  int rpcPort;            /*!< Port of the internal JSON-RPC server to control the IceAdapter */
  int gpgNetPort;         /*!< Port of the internal GPGNet server to communicate with the game */
  int gameUdpPort;        /*!< UDP port the game should use to communicate to the internal Relays */
  std::string logDirectory;    /*!< an optional file loggin directory, default: "" - no file log */
  std::string logLevel;   /*!< logging verbosity level, default: "debug"*/

  /** \brief Create an options object from cmd arguments
      */
  static IceAdapterOptions init(int argc, char *argv[]);
  static IceAdapterOptions init(int id, std::string const& login);
protected:

  IceAdapterOptions();

};

}
