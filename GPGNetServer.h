#pragma once

#include <functional>

#include "TcpServer.h"

namespace Json
{
  class Value;
}

namespace faf
{

struct GPGNetMessage;

enum class InitMode : unsigned int
{
  NormalLobby = 0,
  AutoLobby = 1
};

class GPGNetServer : public TcpServer
{
public:
  /** \brief create a GPGNetServer instance
       \param port: The port for the TCP server
      */
  GPGNetServer(int port);
  virtual ~GPGNetServer();

  /** \brief send an arbitrary message to the client
       \param msg: The message to send
      */
  void sendMessage(GPGNetMessage const& msg);

  /** \brief Tells the client to create a new LobbyComm instance and have it listen on the given port number
       \param init_mode: Whether to use ranked or ladder mode for the in game lobby
       \param port: The port number for the client to listen on
       \param login: The username of the player
       \param playerId: The identifier of the player
       \param natTraversalProvider: A number representing the nat-traversal-provider, typically 1
      */
  void sendCreateLobby(InitMode initMode,
                       int port,
                       std::string const& login,
                       int playerId,
                       int natTraversalProvider);

  /** \brief Tells a client that has a listening LobbyComm instance to connect to the given peer
      \param addressAndPort: String of the form "adress:port"
      \param playerName: Remote player name
      \param playerId: Remote player identifier
      */
  void sendConnectToPeer(std::string const& addressAndPort,
                         std::string const& playerName,
                         int playerId);

  /** \brief Tells the game to join the given peer by address_and_port
      \param addressAndPort: String of the form "adress:port"
      \param remotePlayerName: Remote player name
      \param remotePlayerId: Remote player identifier
      */
  void sendJoinGame(std::string const& addressAndPort,
                    std::string const& remotePlayerName,
                    int remotePlayerId);

  /** \brief Tells the game to start listening for incoming connections as a host
      \param map: Which scenario to use
      */
  void sendHostGame(std::string const& map);

  /** \brief Instructs the game to send a nat-traversal UDP packet to the given remote address and port.
             The game will send the message verbatim as UDP-datagram prefixed with a \0x08 byte.
      \param addressAndPort: String of the form "adress:port"
      \param message: some payload
      */
  void sendSendNatPacket(std::string const& addressAndPort,
                         std::string const& message);

  /** \brief Instructs the game to disconnect from the peer given by id
      \param remotePlayerId: Remote player identifier
      */
  void sendDisconnectFromPeer(int remotePlayerId);

  /** \brief Heartbeat pinging used between the FAF client and server
      */
  void sendPing();

  typedef std::function<void (GPGNetMessage const&)> GpgMessageCallback;
  void addGpgMessageCallback(GpgMessageCallback cb);

protected:
  virtual void parseMessage(Socket* session, std::vector<char>& msgBuffer) override;

  std::vector<GpgMessageCallback> mGPGNetMessageCallbacks;
};

}
