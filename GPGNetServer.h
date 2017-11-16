#pragma once

#include <memory>
#include <string>
#include <array>

#include <webrtc/rtc_base/asyncsocket.h>

#include "GPGNetMessage.h"

namespace faf {

enum class InitMode : unsigned int
{
  NormalLobby = 0,
  AutoLobby = 1
};

class GPGNetServer : public sigslot::has_slots<>
{
public:
  GPGNetServer();

  void listen(int port);

  int listenPort() const;

  bool hasConnectedClient() const;

  void sendMessage(GPGNetMessage const& msg);

  void sendCreateLobby(InitMode initMode,
                       int port,
                       std::string const& login,
                       int playerId,
                       int natTraversalProvider);

  void sendConnectToPeer(std::string const& addressAndPort,
                         std::string const& playerName,
                         int playerId);

  void sendJoinGame(std::string const& addressAndPort,
                    std::string const& remotePlayerName,
                    int remotePlayerId);

  void sendHostGame(std::string const& map);

  void sendSendNatPacket(std::string const& addressAndPort,
                         std::string const& message);

  void sendDisconnectFromPeer(int remotePlayerId);

  void sendPing();

  sigslot::signal1<GPGNetMessage const&,
                   sigslot::multi_threaded_local> SignalNewGPGNetMessage;
  sigslot::signal0<sigslot::multi_threaded_local> SignalClientConnected;
  sigslot::signal0<sigslot::multi_threaded_local> SignalClientDisconnected;
protected:
  void _onNewClient(rtc::AsyncSocket* socket);
  void _onClientDisconnect(rtc::AsyncSocket* socket, int);
  void _onRead(rtc::AsyncSocket* socket);
  std::unique_ptr<rtc::AsyncSocket> _server;
  std::unique_ptr<rtc::AsyncSocket> _connectedSocket;
  std::array<char, 2048> _readBuffer;
  std::string _currentMsg;
};

} // namespace faf
