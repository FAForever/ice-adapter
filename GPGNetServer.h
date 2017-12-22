#pragma once

#include <memory>
#include <string>
#include <array>

#include <webrtc/rtc_base/sigslot.h>
#include <webrtc/rtc_base/asyncsocket.h>
#include <webrtc/rtc_base/messagehandler.h>

#include "GPGNetMessage.h"

namespace faf {

enum class InitMode : unsigned int
{
  NormalLobby = 0,
  AutoLobby = 1
};

class GPGNetConnectionHandler : public sigslot::has_slots<>
{
public:
  GPGNetConnectionHandler(rtc::AsyncSocket* socket);

  void send(std::string const& msg);

  sigslot::signal1<GPGNetMessage, sigslot::multi_threaded_local> SignalNewGPGNetMessage;
  sigslot::signal1<GPGNetConnectionHandler*, sigslot::multi_threaded_local> SignalClientDisconnected;

protected:
  void _onClientDisconnect(rtc::AsyncSocket* socket, int);
  void _onRead(rtc::AsyncSocket* socket);

  rtc::AsyncSocket* _socket;
  std::array<char, 2048> _readBuffer;
  std::string _currentMsg;
  RTC_DISALLOW_COPY_AND_ASSIGN(GPGNetConnectionHandler);
};

class GPGNetServer : public sigslot::has_slots<>, public rtc::MessageHandler
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

  sigslot::signal1<GPGNetMessage, sigslot::multi_threaded_local> SignalNewGPGNetMessage;
  sigslot::signal0<sigslot::multi_threaded_local> SignalClientConnected;
  sigslot::signal0<sigslot::multi_threaded_local> SignalClientDisconnected;
protected:
  void _onNewClient(rtc::AsyncSocket* socket);
  void _onClientDisconnect(GPGNetConnectionHandler* handler);
  void _onClientMessage(GPGNetMessage msg);
  void _onRead(rtc::AsyncSocket* socket);
  virtual void OnMessage(rtc::Message* msg) override;
  std::unique_ptr<rtc::AsyncSocket> _server;
  std::set<GPGNetConnectionHandler*> _connectedSockets;

  RTC_DISALLOW_COPY_AND_ASSIGN(GPGNetServer);
};


} // namespace faf
