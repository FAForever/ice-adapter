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
  int iceLocalPortMin;    /*!< Start of port range to use for ICE local host candidates */
  int iceLocalPortMax;    /*!< End of port range to use for ICE local host candidates */
  bool useUpnp;           /*!< Use UPNP for NAT port configuration */
  int gpgNetPort;         /*!< Port of the internal GPGNet server to communicate with the game */
  int gameUdpPort;        /*!< UDP port the game should use to communicate to the internal Relays */
  std::string stunHost;   /*!< STUN hostname for ICE, default: dev.faforever.com */
  std::string turnHost;   /*!< TURN hostname for ICE, default: dev.faforever.com */
  std::string turnUser;   /*!< TURN user credential for ICE, default: empty */
  std::string turnPass;   /*!< TURN password credential for ICE, default: empty */
  std::string logFile;    /*!< an optional file log, default: "" - no file log */

  /** \brief Create an options object from cmd arguments
      */
  static IceAdapterOptions init(int argc, char *argv[]);
protected:

  IceAdapterOptions();

};

}
