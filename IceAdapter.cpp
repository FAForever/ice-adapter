#include "IceAdapter.h"

#include <boost/log/trivial.hpp>

#include <json/json.h>

#include "GPGNetServer.h"
#include "JsonRpcTcpServer.h"
#include "PeerRelay.h"

IceAdapterOptions::IceAdapterOptions()
{
  stunHost          = "dev.faforever.com";
  turnHost          = "dev.faforever.com";
  turnUser          = "";
  turnPass          = "";
  rpcPort           = 7236;
  gpgNetPort        = 7237;
  gameUdpPort       = 7238;
  relayUdpPortStart = 7240;
  localPlayerId     = 96114;
  localPlayerLogin  = "sandwormsurfer";
}

IceAdapter::IceAdapter(IceAdapterOptions const& options,
                       Glib::RefPtr<Glib::MainLoop> mainloop):
  mOptions(options),
  mMainloop(mainloop),
  mCurrentRelayPort(options.relayUdpPortStart)
{
  mRpcServer    = std::make_shared<JsonRpcTcpServer>(options.rpcPort);
  mGPGNetServer = std::make_shared<GPGNetServer>(options.gpgNetPort);
  mGPGNetServer->setGpgNetCallback("GameState",
                                   std::bind(&IceAdapter::onGpgNetGamestate,
                                             this,
                                             std::placeholders::_1));

  connectRpcMethods();

  auto resolver = Gio::Resolver::get_default();
  {
    auto addresses = resolver->lookup_by_name(mOptions.stunHost);
    if (addresses.size() == 0)
    {
      throw std::runtime_error("error looking up STUN hostname");
    }
    mStunIp = (*addresses.begin())->to_string();
  }
  {
    auto addresses = resolver->lookup_by_name(mOptions.turnHost);
    if (addresses.size() == 0)
    {
      throw std::runtime_error("error looking up TURN hostname");
    }
    mTurnIp = (*addresses.begin())->to_string();
  }
}

void IceAdapter::hostGame(std::string const& map)
{
  if (!mHostGameMap.empty())
  {
    throw std::runtime_error(std::string("already hosting map ") + mHostGameMap);
  }
  if (!mJoinGameRemotePlayerLogin.empty())
  {
    throw std::runtime_error(std::string("already joining game of ") + mJoinGameRemotePlayerLogin);
  }
  mHostGameMap = map;

  mGPGNetServer->sendCreateLobby(InitMode::NormalLobby,
                                 mOptions.gameUdpPort,
                                 mOptions.localPlayerLogin,
                                 mOptions.localPlayerId,
                                 1);
}

void IceAdapter::joinGame(std::string const& remotePlayerLogin,
              int remotePlayerId)
{
  if (!mHostGameMap.empty())
  {
    throw std::runtime_error(std::string("already hosting map ") + mHostGameMap);
  }
  if (!mJoinGameRemotePlayerLogin.empty())
  {
    throw std::runtime_error(std::string("already joining game of ") + mJoinGameRemotePlayerLogin);
  }
  mJoinGameRemotePlayerLogin = remotePlayerLogin;
  mJoinGameRemotePlayerId = remotePlayerId;

  mGPGNetServer->sendCreateLobby(InitMode::NormalLobby,
                                 mOptions.gameUdpPort,
                                 mOptions.localPlayerLogin,
                                 mOptions.localPlayerId,
                                 1);

  Json::Value needSdpParams(Json::arrayValue);
  needSdpParams.append(mOptions.localPlayerId);
  needSdpParams.append(remotePlayerId);
  mRpcServer->sendRequest("rpcNeedSdp",
                          needSdpParams);

  auto port = mCurrentRelayPort++;
  auto relay = std::make_shared<PeerRelay>(mMainloop,
                                           port,
                                           mOptions.gameUdpPort,
                                           remotePlayerId,
                                           mStunIp,
                                           mTurnIp,
                                           mOptions.turnUser,
                                           mOptions.turnPass);
  relay->gatherCandidates([this, remotePlayerId](PeerRelay* relay, std::string const& sdp)
  {
    Json::Value gatheredSdpParams(Json::arrayValue);
    gatheredSdpParams.append(mOptions.localPlayerId);
    gatheredSdpParams.append(remotePlayerId);
    gatheredSdpParams.append(sdp);
    mRpcServer->sendRequest("rpcGatheredSdp",
                            gatheredSdpParams);
  });
  mRelays[remotePlayerId] = relay;

  mGPGNetServer->sendJoinGame(std::string("127.0.0.1:") + std::to_string(port),
                              remotePlayerLogin,
                              remotePlayerId);
}

void IceAdapter::onGpgNetGamestate(std::vector<Json::Value> const& chunks)
{
  if (chunks.size() == 1)
  {
    {
      Json::Value stateChangeParams(Json::arrayValue);
      stateChangeParams.append(chunks[0].asString());
      mRpcServer->sendRequest("rpcGamestateChanged",
                              stateChangeParams);
    };
    if (chunks[0].asString() == "Lobby")
    {
      if (!mHostGameMap.empty())
      {
        mGPGNetServer->sendHostGame(mHostGameMap);
      }
    }
  }
}

void IceAdapter::connectRpcMethods()
{
  if (!mRpcServer)
  {
    return;
  }

  mRpcServer->setRpcCallback("quit",
                             [this](Json::Value const& paramsArray,
                                    Json::Value & result,
                                    Json::Value & error)
  {
    result = "ok";
    mMainloop->quit();
  });

  mRpcServer->setRpcCallback("hostGame",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error)
  {
    if (paramsArray.size() < 1)
    {
      error = "Need 1 parameter: mapName (string)";
      return;
    }
    try
    {
      hostGame(paramsArray[0].asString());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  mRpcServer->setRpcCallback("joinGame",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: remotePlayerLogin (string), remotePlayerId (int)";
      return;
    }
    try
    {
      joinGame(paramsArray[0].asString(), paramsArray[1].asInt());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });
}
