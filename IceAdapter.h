#pragma once

#include <string>
#include <memory>
#include <vector>

#include <glibmm.h>

struct IceAdapterOptions
{
  Glib::ustring stunHost;
  Glib::ustring turnHost;
  Glib::ustring turnUser;
  Glib::ustring turnPass;
  int rpcPort;
  int gpgNetPort;
  int gameUdpPort;
  int relayUdpPortStart;
  int localPlayerId;
  Glib::ustring localPlayerLogin;

  IceAdapterOptions();
};

class JsonRpcTcpServer;
class GPGNetServer;
class PeerRelay;
namespace Json
{
  class Value;
}

class IceAdapter
{
public:
  IceAdapter(IceAdapterOptions const& options,
             Glib::RefPtr<Glib::MainLoop> mainloop);

  void hostGame(std::string const& map);
  void joinGame(std::string const& remotePlayerLogin,
                int remotePlayerId);
protected:
  void onGpgNetGamestate(std::vector<Json::Value> const& chunks);

  void connectRpcMethods();

  std::shared_ptr<JsonRpcTcpServer> mRpcServer;
  std::shared_ptr<GPGNetServer> mGPGNetServer;
  IceAdapterOptions mOptions;
  Glib::RefPtr<Glib::MainLoop> mMainloop;

  std::string mStunIp;
  std::string mTurnIp;

  std::string mHostGameMap;
  std::string mJoinGameRemotePlayerLogin;
  int mJoinGameRemotePlayerId;

  int mCurrentRelayPort;
  std::map<int, std::shared_ptr<PeerRelay>> mRelays;
};
