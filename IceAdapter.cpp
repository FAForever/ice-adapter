#include "IceAdapter.h"

#include <json/json.h>

#include "GPGNetServer.h"
#include "GPGNetMessage.h"
#include "JsonRpcServer.h"
#include "PeerRelay.h"
#include "IceAgent.h"
#include "logging.h"

namespace faf
{

const int DUMMY_RESERVED_PLAYER_ID = -1;

IceAdapter::IceAdapter(IceAdapterOptions const& options,
                       Glib::RefPtr<Glib::MainLoop> mainloop):
  mOptions(options),
  mMainloop(mainloop),
  mTaskState(IceAdapterTaskState::NoTask)
{
  FAF_LOG_INFO << "ICE adapter version " << FAF_VERSION_STRING << " initializing";
  mRpcServer    = std::make_shared<JsonRpcServer>(mOptions.rpcPort);
  mGPGNetServer = std::make_shared<GPGNetServer>(mOptions.gpgNetPort);
  mGPGNetServer->addGpgMessageCallback(std::bind(&IceAdapter::onGpgNetMessage,
                                       this,
                                       std::placeholders::_1));
  mGPGNetServer->connectionChanged.connect(std::bind(&IceAdapter::onGpgConnectionStateChanged,
                                                     this));
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
  if (mTaskState != IceAdapterTaskState::NoTask)
  {
    throw std::runtime_error(std::string("joinGame/hostGame may only called once per game connection session. Wait for the game to disconnect."));
  }
  mHostGameMap = map;
  mTaskState = IceAdapterTaskState::ShouldHostGame;
  tryExecuteTask();
}

void IceAdapter::joinGame(std::string const& remotePlayerLogin,
                          int remotePlayerId)
{
  if (mTaskState != IceAdapterTaskState::NoTask)
  {
    throw std::runtime_error(std::string("joinGame/hostGame may only called once per game connection session. Wait for the game to disconnect."));
  }
  mJoinGameRemotePlayerLogin = remotePlayerLogin;
  mJoinGameRemotePlayerId = remotePlayerId;
  mTaskState = IceAdapterTaskState::ShouldJoinGame;
  tryExecuteTask();
}

void IceAdapter::connectToPeer(std::string const& remotePlayerLogin,
                               int remotePlayerId,
                               bool createOffer)
{
  FAF_LOG_TRACE << "connectToPeer " << remotePlayerLogin << " " << remotePlayerId;
  if (mTaskState == IceAdapterTaskState::ShouldJoinGame ||
      mTaskState == IceAdapterTaskState::SentJoinGame)
  {
    if (mRelays.count(remotePlayerId) == 0)
    {
      FAF_LOG_ERROR << "connectToPeer called in Join-mode: existing relay expected";
    }
  }
  int relayPort;
  auto relay = createPeerRelayOrUseReserved(remotePlayerId,
                                            remotePlayerLogin,
                                            relayPort,
                                            createOffer);
  if (relay)
  {
    mRelays[remotePlayerId] = relay;
    mGPGNetServer->sendConnectToPeer(std::string("127.0.0.1:") + std::to_string(relayPort),
                                     remotePlayerLogin,
                                     remotePlayerId);
  }
}

void IceAdapter::reconnectToPeer(int remotePlayerId)
{
  auto relayIt = mRelays.find(remotePlayerId);
  if (relayIt == mRelays.end())
  {
    FAF_LOG_ERROR << "no relay for remote peer " << remotePlayerId << " found";
    std::string errorMsg("no relay for remote peer ");
    errorMsg += std::to_string(remotePlayerId);
    errorMsg += " found. Please call joinGame() or connectToPeer() first";
    throw std::runtime_error(errorMsg);
  }
  relayIt->second->reconnect();
}

void IceAdapter::disconnectFromPeer(int remotePlayerId)
{
  auto relayIt = mRelays.find(remotePlayerId);
  if (relayIt == mRelays.end())
  {
    FAF_LOG_ERROR << "no relay for remote peer " << remotePlayerId << " found";
    std::string errorMsg("no relay for remote peer ");
    errorMsg += std::to_string(remotePlayerId);
    errorMsg += " found. Please call joinGame() or connectToPeer() first";
    throw std::runtime_error(errorMsg);
  }
  mGPGNetServer->sendDisconnectFromPeer(remotePlayerId);
  mRelays.erase(relayIt);
}

void IceAdapter::addSdpMessage(int remotePlayerId, std::string const& type, std::string const& msg)
{
  auto relayIt = mRelays.find(remotePlayerId);
  if (relayIt == mRelays.end())
  {
    FAF_LOG_ERROR << "no relay for remote peer " << remotePlayerId << " found";
    std::string errorMsg("no relay for remote peer ");
    errorMsg += std::to_string(remotePlayerId);
    errorMsg += " found. Please call joinGame() or connectToPeer() first";
    throw std::runtime_error(errorMsg);
  }
  if(!relayIt->second->iceAgent())
  {
    FAF_LOG_ERROR << "!relayIt->second->iceAgent()";
    throw std::runtime_error("!relayIt->second->iceAgent()");
  }
  if(relayIt->second->iceAgent()->isConnected())
  {
    FAF_LOG_WARN << "relayIt->second->iceAgent()->isConnected()";
  }
  relayIt->second->iceAgent()->addRemoteSdpMessage(type, msg);
}

void IceAdapter::sendToGpgNet(GPGNetMessage const& message)
{
  mGPGNetServer->sendMessage(message);
}

Json::Value IceAdapter::status() const
{
  Json::Value result;
  result["version"] = FAF_VERSION_STRING;
  /* options */
  {
    Json::Value options;

    options["player_id"]            = mOptions.localPlayerId;
    options["player_login"]         = std::string(mOptions.localPlayerLogin);
    options["rpc_port"]             = mOptions.rpcPort;
    options["ice_local_port_min"]   = mOptions.iceLocalPortMin;
    options["ice_local_port_max"]   = mOptions.iceLocalPortMax;
    options["use_upnp"]             = mOptions.useUpnp;
    options["gpgnet_port"]          = mOptions.gpgNetPort;
    options["game_udp_port"]        = mOptions.gameUdpPort;
    options["stun_host"]            = std::string(mOptions.stunHost);
    options["turn_host"]            = std::string(mOptions.turnHost);
    options["turn_user"]            = std::string(mOptions.turnUser);
    options["turn_pass"]            = std::string(mOptions.turnPass);
    options["log_file"]             = std::string(mOptions.logFile);
    result["options"] = options;
  }
  /* GPGNet */
  {
    Json::Value gpgnet;

    gpgnet["local_port"] = mGPGNetServer->listenPort();
    gpgnet["connected"] = mGPGNetServer->sessionCount() > 0;
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
      relay["remote_player_login"] = it->second->peerLogin();
      relay["local_game_udp_port"] = it->second->localGameUdpPort();

      if (it->second->iceAgent())
      {
        relay["ice_agent"]["state"] = stateToString(it->second->iceAgent()->state());
        relay["ice_agent"]["connected"] = it->second->iceAgent()->isConnected();
        relay["ice_agent"]["local_candidate"] = it->second->iceAgent()->localCandidateInfo();
        relay["ice_agent"]["remote_candidate"] = it->second->iceAgent()->remoteCandidateInfo();
        relay["ice_agent"]["local_sdp"] = it->second->iceAgent()->localSdp();
        relay["ice_agent"]["local_sdp64"] = it->second->iceAgent()->localSdp64();
        relay["ice_agent"]["remote_sdp"] = it->second->iceAgent()->remoteSdp();
      }

      relays.append(relay);
    }
    result["relays"] = relays;
  }
  return result;
}

void IceAdapter::reserveRelays(int count)
{
  int newRelays = count - int(mReservedRelays.size() + mRelays.size());
  if (newRelays > 0)
  {
    FAF_LOG_DEBUG << "reserving " << newRelays << " relays";
  }
  for (int i = 0; i < newRelays; ++i)
  {
    int relayPort;
    auto relay = createPeerRelay(DUMMY_RESERVED_PLAYER_ID,
                                 "",
                                 relayPort,
                                 true);
    if (relay)
    {
      mReservedRelays.push(relay);
    }
  }
}

void IceAdapter::onGpgNetMessage(GPGNetMessage const& message)
{
  if (message.header == "GameState")
  {
    if (message.chunks.size() == 1)
    {
      mGPGNetGameState = message.chunks[0].asString();
      if (mGPGNetGameState == "Idle")
      {
        mGPGNetServer->sendCreateLobby(InitMode::NormalLobby,
                                       mOptions.gameUdpPort,
                                       mOptions.localPlayerLogin,
                                       mOptions.localPlayerId,
                                       1);
      }
      tryExecuteTask();
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

void IceAdapter::onGpgConnectionStateChanged()
{
  Json::Value params(Json::arrayValue);
  params.append(mRpcServer->sessionCount() > 0 ? "Connected" : "Disconnected");
  mRpcServer->sendRequest("onConnectionStateChanged",
                          params);
  if (mRpcServer->sessionCount() == 0)
  {
    FAF_LOG_TRACE << "game disconnected";

    mHostGameMap = "";
    mJoinGameRemotePlayerLogin = "";
    mRelays.clear();
    mGPGNetGameState = "";
    mTaskState = IceAdapterTaskState::NoTask;
  }
  else
  {
    FAF_LOG_TRACE << "game connected";
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
                                    Json::Value & error,
                                    Socket* session)
  {
    result = "ok";
    mMainloop->quit();
  });

  mRpcServer->setRpcCallback("hostGame",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             Socket* session)
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
                             Json::Value & error,
                             Socket* session)
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
                             Json::Value & error,
                             Socket* session)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: remotePlayerLogin (string), remotePlayerId (int)";
      return;
    }
    try
    {
      connectToPeer(paramsArray[0].asString(),
                    paramsArray[1].asInt(),
                    paramsArray.size() > 2 ? paramsArray[2].asBool() : true);
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  mRpcServer->setRpcCallback("reconnectToPeer",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             Socket* session)
  {
    if (paramsArray.size() < 1)
    {
      error = "Need 1 parameters: remotePlayerId (int)";
      return;
    }
    try
    {
      reconnectToPeer(paramsArray[0].asInt());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  mRpcServer->setRpcCallback("disconnectFromPeer",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             Socket* session)
  {
    if (paramsArray.size() < 1)
    {
      error = "Need 1 parameters: remotePlayerId (int)";
      return;
    }
    try
    {
      disconnectFromPeer(paramsArray[0].asInt());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  mRpcServer->setRpcCallback("addSdpMessage",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             Socket* session)
  {
    if (paramsArray.size() < 3)
    {
      error = "Need 3 parameters: remotePlayerId (int), type (string), msg (string)";
      return;
    }
    try
    {
      addSdpMessage(paramsArray[0].asInt(),
                    paramsArray[1].asString(),
                    paramsArray[2].asString());
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
                             Json::Value & error,
                             Socket* session)
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
                             Json::Value & error,
                             Socket* session)
  {
    result = status();
  });

  mRpcServer->setRpcCallback("reserveRelays",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             Socket* session)
  {
    if (paramsArray.size() < 1)
    {
      error = "Need 1 parameters: count (int)";
      return;
    }
    try
    {
      reserveRelays(paramsArray[0].asInt());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });
}

void IceAdapter::tryExecuteTask()
{
  switch (mTaskState)
  {
    case IceAdapterTaskState::NoTask:
      return;
    case IceAdapterTaskState::ShouldJoinGame:
      if (mGPGNetGameState == "Lobby")
      {
        int relayPort;
        auto relay = createPeerRelayOrUseReserved(mJoinGameRemotePlayerId,
                                                  mJoinGameRemotePlayerLogin,
                                                  relayPort,
                                                  false);

        if (relay)
        {
          mRelays[mJoinGameRemotePlayerId] = relay;
          mGPGNetServer->sendJoinGame(std::string("127.0.0.1:") + std::to_string(relayPort),
                                      mJoinGameRemotePlayerLogin,
                                      mJoinGameRemotePlayerId);
        }
      }
      mTaskState = IceAdapterTaskState::SentJoinGame;
      break;
    case IceAdapterTaskState::SentJoinGame:
      return;
    case IceAdapterTaskState::ShouldHostGame:
      if (mGPGNetGameState == "Lobby")
      {
        mGPGNetServer->sendHostGame(mHostGameMap);
      }
      mTaskState = IceAdapterTaskState::SentHostGame;
      break;
    case IceAdapterTaskState::SentHostGame:
      return;
  }
}

std::shared_ptr<PeerRelay> IceAdapter::createPeerRelayOrUseReserved(int remotePlayerId,
                                                        std::string const& remotePlayerLogin,
                                                        int& portResult,
                                                        bool createOffer)
{
  if (!mReservedRelays.empty())
  {
    std::shared_ptr<PeerRelay> relay = mReservedRelays.front();
    mReservedRelays.pop();
    relay->setPeer(remotePlayerId,
                   remotePlayerLogin);
    portResult = relay->localGameUdpPort();

    /* if we already gathered a SDP for this reserved relay, forward it on next tick */
    Glib::signal_idle().connect_once([this, relay]()
    {
      if (!relay->iceAgent()->localSdp64().empty())
      {
        Json::Value gatheredSdpParams(Json::arrayValue);
        gatheredSdpParams.append(mOptions.localPlayerId);
        gatheredSdpParams.append(relay->peerId());
        gatheredSdpParams.append(relay->iceAgent()->localSdp64());
        mRpcServer->sendRequest("onSdpGathered",
                                gatheredSdpParams);
      }
    }, 0);
    return relay;
  }
  else
  {
    return createPeerRelay(remotePlayerId,
                           remotePlayerLogin,
                           portResult,
                           createOffer);
  }
}

std::shared_ptr<PeerRelay> IceAdapter::createPeerRelay(int remotePlayerId,
                                                       std::string const& remotePlayerLogin,
                                                       int& portResult,
                                                       bool createOffer)
{

  auto sdpMsgCb = [this](PeerRelay* relay, std::string const& type, std::string const& msg)
  {
    /* Only non-reserved relays will be forwarded */
    if (relay->peerId() != DUMMY_RESERVED_PLAYER_ID)
    {
      Json::Value gatheredSdpParams(Json::arrayValue);
      gatheredSdpParams.append(mOptions.localPlayerId);
      gatheredSdpParams.append(relay->peerId());
      gatheredSdpParams.append(type);
      gatheredSdpParams.append(msg);
      mRpcServer->sendRequest("onSdpMessage",
                              gatheredSdpParams);
    }
  };

  auto stateCb = [this](PeerRelay* relay, IceAgentState const& state)
  {
    /* Only non-reserved relays will be forwarded */
    if (relay->peerId() != DUMMY_RESERVED_PLAYER_ID)
    {
      Json::Value iceStateParams(Json::arrayValue);
      iceStateParams.append(mOptions.localPlayerId);
      iceStateParams.append(relay->peerId());
      iceStateParams.append(stateToString(state));
      mRpcServer->sendRequest("onPeerStateChanged",
                              iceStateParams);
    }
  };

  auto result = std::make_shared<PeerRelay>(mMainloop,
                                            remotePlayerId,
                                            remotePlayerLogin,
                                            mStunIp,
                                            mTurnIp,
                                            sdpMsgCb,
                                            stateCb,
                                            createOffer,
                                            mOptions);
  portResult = result->localGameUdpPort();

  if (createOffer &&
      remotePlayerId != DUMMY_RESERVED_PLAYER_ID)
  {
    result->iceAgent()->gatherCandidates();
  }

  return result;
}

}
