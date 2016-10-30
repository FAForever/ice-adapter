#pragma once

#include <memory>

#include <glibmm.h>

typedef std::shared_ptr<class IceAdapterOptions> IceAdapterOptionsPtr;

/*! \brief Options struct
 */
class IceAdapterOptions : public Glib::OptionGroup
{
public:
  int localPlayerId;      /*!< ID of the local player */
  Glib::ustring localPlayerLogin; /*!< Login of the local player */
  int rpcPort;            /*!< Port of the internal JSON-RPC server to control the IceAdapter */
  int gpgNetPort;         /*!< Port of the internal GPGNet server to communicate with the game */
  int gameUdpPort;        /*!< UDP port the game should use to communicate to the internal Relays */
  int relayUdpPortStart;  /*!< first UDP port the IceAdapter should use for the Relays */
  Glib::ustring stunHost; /*!< STUN hostname for ICE, default: dev.faforever.com */
  Glib::ustring turnHost; /*!< TURN hostname for ICE, default: dev.faforever.com */
  Glib::ustring turnUser; /*!< TURN user credential for ICE, default: empty */
  Glib::ustring turnPass; /*!< TURN password credential for ICE, default: empty */

  /** \brief Create an options object from cmd arguments
      */
  static IceAdapterOptionsPtr init(int argc, char *argv[]);
protected:

  IceAdapterOptions();

};
