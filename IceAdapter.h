#pragma once

#include <string>
#include <memory>
#include <queue>

#include <glibmm.h>

#include "IceAdapterOptions.h"

/* Forward declarations */
class JsonRpcTcpServer;
enum class ConnectionState;
class GPGNetServer;
class GPGNetMessage;
class PeerRelay;
namespace Json
{
  class Value;
}

enum class IceAdapterTaskState
{
  NoTask,
  ShouldJoinGame,
  SentJoinGame,
  ShouldHostGame,
  SentHostGame
};

/*! \brief The main controller class
 *
 *  IceAdapter opens the JsonRpcTcpServer to be controlled
 *  by the game client.
 *  Creates the GPGNetServer for communication with the game.
 *  Creates one Relay for every Peer.
 */
class IceAdapter
{
public:
  IceAdapter(IceAdapterOptions const& options,
             Glib::RefPtr<Glib::MainLoop> mainloop);

  /** \brief Sets the IceAdapter in hosting mode and tells the connected game to host the map once
   *         it reaches "Lobby" state
       \param map: Map to host
      */
  void hostGame(std::string const& map);

  /** \brief Sets the IceAdapter in join mode and connects to the hosted lobby
   *         The IceAdapter will implicitly create a Relay for the remote player.
   *         The IceAdapter will ask the client for an SDP record of the remote peer via "rpcNeedSdp" JSONRPC notification.
   *         The IceAdapter will send the client the SDP record via "rpcGatheredSdp" JSONRPC notification.
       \param remotePlayerLogin: Login name of the player hosting the Lobby
       \param remotePlayerId:    ID of the player hosting the Lobby
      */
  void joinGame(std::string const& remotePlayerLogin,
                int remotePlayerId);

  /** \brief Tell the game to connect to a remote player once it reached Lobby state
   *         The same internal procedures as in @joinGame happen:
   *         The IceAdapter will implicitly create a Relay for the remote player.
   *         The IceAdapter will ask the client for an SDP record of the remote peer via "rpcNeedSdp" JSONRPC notification.
   *         The IceAdapter will send the client the SDP record via "rpcGatheredSdp" JSONRPC notification.
       \param remotePlayerLogin: Login name of the player to connect to
       \param remotePlayerId:    ID of the player to connect to
      */
  void connectToPeer(std::string const& remotePlayerLogin,
                     int remotePlayerId);

  /** \brief Not sure yet
      */
  void reconnectToPeer(int remotePlayerId);

  /** \brief Tell the game to disconnect from a remote peer
   *         Will remove the Relay.
       \param remotePlayerId:    ID of the player to disconnect from
      */
  void disconnectFromPeer(int remotePlayerId);

  /** \brief Sets the SDP record for the remote peer.
   *         This method assumes a previous call of joinGame or connectToPeer.
       \param remotePlayerId: ID of the remote player
       \param sdp64:          Base64 encoded sdp record
      */
  void setSdp(int remotePlayerId, std::string const& sdp64);

  /** \brief Send an arbitrary GPGNet message to the game
       \param message: The GPGNet message
      */
  void sendToGpgNet(GPGNetMessage const& message);

  /** \brief Return the ICEAdapters status
       \returns The status as JSON structure
      */
  Json::Value status() const;

  /** \brief Reserves relays for future use
   *         Note that all currently running relays are already count as reserved
       \param count: The accumulated number of reserved relays
      */
  void reserveRelays(int count);
protected:
  void onGpgNetMessage(GPGNetMessage const& message);
  void onGpgConnectionStateChanged(ConnectionState const& s);

  void connectRpcMethods();

  void tryExecuteTask();

  std::shared_ptr<PeerRelay> createPeerRelayOrUseReserved(int remotePlayerId,
                                                          std::string const& remotePlayerLogin,
                                                          int& portResult);

  std::shared_ptr<PeerRelay> createPeerRelay(int remotePlayerId,
                                             std::string const& remotePlayerLogin,
                                             int& portResult);

  IceAdapterOptions mOptions;
  std::shared_ptr<JsonRpcTcpServer> mRpcServer;
  std::shared_ptr<GPGNetServer> mGPGNetServer;
  Glib::RefPtr<Glib::MainLoop> mMainloop;

  std::string mStunIp;
  std::string mTurnIp;

  std::string mHostGameMap;
  std::string mJoinGameRemotePlayerLogin;
  int mJoinGameRemotePlayerId;

  std::string mGPGNetGameState;

  std::map<int, std::shared_ptr<PeerRelay>> mRelays;
  std::queue<std::shared_ptr<PeerRelay>> mReservedRelays;

  IceAdapterTaskState mTaskState;
};
