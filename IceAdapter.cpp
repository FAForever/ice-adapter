#include "IceAdapter.h"

#include <boost/log/trivial.hpp>

#include <json/json.h>

#include "GPGNetServer.h"
#include "GPGNetMessage.h"
#include "JsonRpcTcpServer.h"
#include "PeerRelay.h"
#include "IceAgent.h"

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
  mGPGNetServer->addGpgMessageCallback(std::bind(&IceAdapter::onGpgNetMessage,
                                       this,
                                       std::placeholders::_1));
  mGPGNetServer->addConnectionStateCallback(std::bind(&IceAdapter::onGpgConnectionStateChanged,
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

  int relayPort;
  auto relay = createPeerRelay(remotePlayerId,
                               relayPort);

  if (relay)
  {
    mGPGNetServer->sendJoinGame(std::string("127.0.0.1:") + std::to_string(relayPort),
                                remotePlayerLogin,
                                remotePlayerId);
  }
}

void IceAdapter::connectToPeer(std::string const& remotePlayerLogin,
                               int remotePlayerId)
{
  int relayPort;
  auto relay = createPeerRelay(remotePlayerId,
                               relayPort);
  if (relay)
  {
    mGPGNetServer->sendConnectToPeer(std::string("127.0.0.1:") + std::to_string(relayPort),
                                     remotePlayerLogin,
                                     remotePlayerId);
  }
}

void IceAdapter::setSdp(int remotePlayerId, std::string const& sdp64)
{
  auto relayIt = mRelays.find(remotePlayerId);
  if (relayIt == mRelays.end())
  {
    BOOST_LOG_TRIVIAL(error) << "no relay for remote peer " << remotePlayerId << " found";
    std::string errorMsg("no relay for remote peer ");
    errorMsg += std::to_string(remotePlayerId);
    errorMsg += " found. Please call joinGame() or connectToPeer() first";
    throw std::runtime_error(errorMsg);
  }
  if(!relayIt->second->iceAgent())
  {
    BOOST_LOG_TRIVIAL(error) << "!relayIt->second->iceAgent()";
    throw std::runtime_error("!relayIt->second->iceAgent()");
  }
  if(relayIt->second->iceAgent()->isConnected())
  {
    BOOST_LOG_TRIVIAL(warning) << "relayIt->second->iceAgent()->isConnected()";
  }
  relayIt->second->iceAgent()->setRemoteSdp(sdp64);
}

void IceAdapter::sendToGpgNet(GPGNetMessage const& message)
{
  mGPGNetServer->sendMessage(message);
}

Json::Value IceAdapter::status() const
{
  Json::Value result;
  /* options */
  {
    Json::Value options;

    options["stun_host"]            = std::string(mOptions.stunHost);
    options["turn_host"]            = std::string(mOptions.turnHost);
    options["turn_user"]            = std::string(mOptions.turnUser);
    options["turn_pass"]            = std::string(mOptions.turnPass);
    options["rpc_port"]             = mOptions.rpcPort;
    options["gpgnet_port"]          = mOptions.gpgNetPort;
    options["game_udp_port"]        = mOptions.gameUdpPort;
    options["relay_udp_port_start"] = mOptions.relayUdpPortStart;
    options["player_id"]            = mOptions.localPlayerId;
    options["player_login"]         = std::string(mOptions.localPlayerLogin);
    result["options"] = options;
  }
  /* GPGNet */
  {
    Json::Value gpgnet;

    gpgnet["local_port"] = mGPGNetServer->port();
    gpgnet["connected"] = mGPGNetServer->connectionState() == ConnectionState::Connected;
    gpgnet["game_state"] = mGPGNetGameState;

    if (!mHostGameMap.empty())
    {
      gpgnet["host_game"]["map"] = mHostGameMap;
    }
    else if(!mJoinGameRemotePlayerLogin.empty())
    {
      gpgnet["join_game"]["remote_player_login"] = mJoinGameRemotePlayerLogin;
      gpgnet["join_game"]["remote_player_id"] = mJoinGameRemotePlayerId;
    }
    result["gpgnet"] = gpgnet;
  }
  /* Relays */
  {
    Json::Value relays(Json::arrayValue);
    for (auto it = mRelays.begin(), end = mRelays.end(); it != end; ++it)
    {
      Json::Value relay;
      relay["remote_player_id"] = it->first;
      relay["local_game_udp_port"] = it->second->localGameUdpPort();

      if (it->second->iceAgent())
      {
        relay["ice_agent"]["state"] = stateToString(it->second->iceAgent()->state());
        relay["ice_agent"]["connected"] = it->second->iceAgent()->isConnected();
        relay["ice_agent"]["local_candidate"] = it->second->iceAgent()->localCandidateInfo();
        relay["ice_agent"]["remote_candidate"] = it->second->iceAgent()->remoteCandidateInfo();
        relay["ice_agent"]["local_sdp"] = it->second->iceAgent()->localSdp();
        relay["ice_agent"]["local_sdp64"] = it->second->iceAgent()->localSdp64();
        relay["ice_agent"]["remote_sdp64"] = it->second->iceAgent()->remoteSdp64();
      }

      relays.append(relay);
    }
    result["relays"] = relays;
  }
  return result;
}

void IceAdapter::onGpgNetMessage(GPGNetMessage const& message)
{
  if (message.header == "GameState")
  {
    if (message.chunks.size() == 1)
    {
      mGPGNetGameState = message.chunks[0].asString();
      if (message.chunks[0].asString() == "Lobby")
      {
        if (!mHostGameMap.empty())
        {
          mGPGNetServer->sendHostGame(mHostGameMap);
        }
      }
    }
  }
  Json::Value rpcParams(Json::arrayValue);
  rpcParams.append(message.header);
  Json::Value msgChunks(Json::arrayValue);
  for(auto const& chunk : message.chunks)
  {
    msgChunks.append(chunk);
  }
  rpcParams.append(msgChunks);
  mRpcServer->sendRequest("onGpgNetMessageReceived",
                          rpcParams);
}

void IceAdapter::onGpgConnectionStateChanged(ConnectionState const& s)
{
  Json::Value params(Json::arrayValue);
  params.append(s == ConnectionState::Connected ? "Connected" : "Disconnected");
  mRpcServer->sendRequest("onConnectionStateChanged",
                          params);
  if (s == ConnectionState::Disconnected)
  {
    BOOST_LOG_TRIVIAL(trace) << "game disconnected";

    mHostGameMap = "";
    mJoinGameRemotePlayerLogin = "";
    mRelays.clear();
    mGPGNetGameState = "";
  }
  else
  {
    BOOST_LOG_TRIVIAL(trace) << "game connected";
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

  mRpcServer->setRpcCallback("connectToPeer",
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
      connectToPeer(paramsArray[0].asString(), paramsArray[1].asInt());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  mRpcServer->setRpcCallback("setSdp",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: remotePlayerId (int), sdp64 (string)";
      return;
    }
    try
    {
      setSdp(paramsArray[0].asInt(), paramsArray[1].asString());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  mRpcServer->setRpcCallback("sendToGpgNet",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error)
  {
    if (paramsArray.size() < 2 ||
        !paramsArray[1].isArray())
    {
      error = "Need 2 parameters: header (string), chunks (array)";
      return;
    }
    try
    {
      GPGNetMessage message;
      message.header = paramsArray[0].asString();
      for(int i = 0; i < paramsArray[1].size(); ++i)
      {
        message.chunks.push_back(paramsArray[1][i]);
      }
      sendToGpgNet(message);
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });
  mRpcServer->setRpcCallback("status",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error)
  {
    result = status();
  });
}

std::shared_ptr<PeerRelay> IceAdapter::createPeerRelay(int remotePlayerId, int& portResult)
{
  portResult = mCurrentRelayPort++;
  auto result = std::make_shared<PeerRelay>(mMainloop,
                                            portResult,
                                            mOptions.gameUdpPort,
                                            remotePlayerId,
                                            mStunIp,
                                            mTurnIp,
                                            mOptions.turnUser,
                                            mOptions.turnPass);

  Json::Value needSdpParams(Json::arrayValue);
  needSdpParams.append(mOptions.localPlayerId);
  needSdpParams.append(remotePlayerId);
  mRpcServer->sendRequest("onNeedSdp",
                          needSdpParams);

  result->gatherCandidates([this, remotePlayerId](PeerRelay* relay, std::string const& sdp)
  {
    Json::Value gatheredSdpParams(Json::arrayValue);
    gatheredSdpParams.append(mOptions.localPlayerId);
    gatheredSdpParams.append(remotePlayerId);
    gatheredSdpParams.append(sdp);
    mRpcServer->sendRequest("onGatheredSdp",
                            gatheredSdpParams);
  });

  result->setIceAgentStateCallback([this, remotePlayerId](PeerRelay* relay, IceAgentState const& state)
  {
    Json::Value iceStateParams(Json::arrayValue);
    iceStateParams.append(mOptions.localPlayerId);
    iceStateParams.append(remotePlayerId);
    iceStateParams.append(stateToString(state));
    mRpcServer->sendRequest("onIceStateChanged",
                            iceStateParams);
  });
   mRelays[remotePlayerId] = result;

   return result;
}
