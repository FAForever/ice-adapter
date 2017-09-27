#pragma once

#include <queue>
#include <memory>

#include <webrtc/base/scoped_ref_ptr.h>
#include <webrtc/api/peerconnectioninterface.h>

#include "IceAdapterOptions.h"
#include "GPGNetServer.h"
#include "JsonRpcServer.h"
#include "PeerRelay.h"

namespace faf {

struct IceAdapterGameTask
{
  enum
  {
    JoinGame,
    HostGame,
    ConnectToPeer,
    DisconnectFromPeer
  } task;
  std::string hostMap;
  std::string remoteLogin;
  int remoteId;
};

class IceAdapter : public sigslot::has_slots<>
{
public:
  IceAdapter(int argc, char *argv[]);
  virtual ~IceAdapter();

  /** \brief Execute programs mainloop */
  void run();

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
                     int remotePlayerId,
                     bool createOffer);

  /** \brief Tell the game to disconnect from a remote peer
   *         Will remove the Relay.
       \param remotePlayerId:    ID of the player to disconnect from
      */
  void disconnectFromPeer(int remotePlayerId);

  /** \brief Set Lobby mode upfront game launch
       \param initMode: "normal" for normal lobby and "auto" for automatch lobby (aka ladder)
      */
  void setLobbyInitMode(std::string const& initMode);

  /** \brief Add ICE signalling message
       \param remotePlayerId: ID of the remote player
       \param msg: the signalling messages generated from the remote player
      */
  void iceMsg(int remotePlayerId, Json::Value const& msg);

  /** \brief Send an arbitrary GPGNet message to the game
       \param message: The GPGNet message
      */
  void sendToGpgNet(GPGNetMessage const& message);

  /** \brief Set the ICE servers to use (STUN and TURN servers)
       \param servers: the JSON Array containing ICE server list
      */
  void setIceServers(Json::Value const& servers);

  /** \brief Return the ICEAdapters status
       \returns The status as JSON structure
      */
  Json::Value status() const;


protected:
  void _connectRpcMethods();
  void _queueGameTask(IceAdapterGameTask t);
  void _tryExecuteGameTasks();
  void _onGameConnected();
  void _onGameDisconnected();
  void _onGpgNetMessage(GPGNetMessage const& message);
  std::shared_ptr<PeerRelay> _createPeerRelay(int remotePlayerId,
                                              std::string const& remotePlayerLogin,
                                              bool createOffer);

  IceAdapterOptions _options;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _pcfactory;
  GPGNetServer _gpgnetServer;
  JsonRpcServer _jsonRpcServer;
  std::queue<IceAdapterGameTask> _gameTasks;
  std::string _gpgnetGameState;
  std::map<int, std::shared_ptr<PeerRelay>> _relays;
  std::string _gametaskString;
  webrtc::PeerConnectionInterface::IceServers _iceServerList;
  std::string _lobbyInitMode;
};

} // namespace faf
