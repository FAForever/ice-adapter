#include "IceAdapter.h"

#include <json/json.h>

#include "GPGNetServer.h"
#include "GPGNetMessage.h"
#include "JsonRpcServer.h"
#include "PeerRelay.h"
#include "IceAgent.h"
#include "IceStream.h"
#include "logging.h"

namespace faf
{

IceAdapter::IceAdapter(IceAdapterOptions const& options,
                       Glib::RefPtr<Glib::MainLoop> mainloop):
  mOptions(options),
  mMainloop(mainloop)
{
  FAF_LOG_INFO << "ICE adapter version " << FAF_VERSION_STRING << " initializing";

  mAgent = std::make_shared<IceAgent>(mMainloop->gobj(),
                                      mOptions);

  mRpcServer = std::make_shared<JsonRpcServer>(mOptions.rpcPort);
  mRpcServer->listen();
  mGPGNetServer = std::make_shared<GPGNetServer>(mOptions.gpgNetPort);
  mGPGNetServer->addGpgMessageCallback(std::bind(&IceAdapter::onGpgNetMessage,
                                       this,
                                       std::placeholders::_1));
  mGPGNetServer->connectionChanged.connect(std::bind(&IceAdapter::onGpgConnectionStateChanged,
                                                     this,
                                                     std::placeholders::_1,
                                                     std::placeholders::_2));
  mGPGNetServer->listen();
  connectRpcMethods();

  FAF_LOG_TRACE << "IceAdapter initialized";
}

void IceAdapter::hostGame(std::string const& map)
{
  queueGameTask({IceAdapterGameTask::HostGame,
                 map,
                 "",
                 0});
}

void IceAdapter::joinGame(std::string const& remotePlayerLogin,
                          int remotePlayerId)
{
  createPeerRelay(remotePlayerId,
                  remotePlayerLogin);
  queueGameTask({IceAdapterGameTask::JoinGame,
                 "",
                 remotePlayerLogin,
                 remotePlayerId});
}

void IceAdapter::connectToPeer(std::string const& remotePlayerLogin,
                               int remotePlayerId)
{
  createPeerRelay(remotePlayerId,
                  remotePlayerLogin);
  queueGameTask({IceAdapterGameTask::ConnectToPeer,
                 "",
                 remotePlayerLogin,
                 remotePlayerId});
}

void IceAdapter::reconnectToPeer(int remotePlayerId)
{
  auto relayIt = mRelays.find(remotePlayerId);
  if (relayIt == mRelays.end())
  {
    FAF_LOG_ERROR << "no relay for remote peer " << remotePlayerId << " found";
    return;
  }
  relayIt->second->reconnect();
}

void IceAdapter::disconnectFromPeer(int remotePlayerId)
{
  auto relayIt = mRelays.find(remotePlayerId);
  if (relayIt == mRelays.end())
  {
    FAF_LOG_ERROR << "no relay for remote peer " << remotePlayerId << " found";
    return;
  }
  mRelays.erase(relayIt);
  mAgent->removeStream(remotePlayerId);
  FAF_LOG_INFO << "removed relay for peer " << remotePlayerId;
  queueGameTask({IceAdapterGameTask::DisconnectFromPeer,
                 "",
                 "",
                 remotePlayerId});
}

void IceAdapter::addSdp(int remotePlayerId, std::string const& sdp)
{
  auto relayIt = mRelays.find(remotePlayerId);
  if (relayIt == mRelays.end())
  {
    FAF_LOG_ERROR << "no relay for remote peer " << remotePlayerId << " found";
    return;
  }
  if(!relayIt->second->iceStream())
  {
    FAF_LOG_ERROR << "!relayIt->second->iceAgent()";
    return;
  }
  if(relayIt->second->iceStream()->peerConnectedToMe())
  {
    FAF_LOG_WARN << "relayIt->second->iceAgent()->isConnected()";
  }
  relayIt->second->iceStream()->addRemoteSdp(sdp);
}

void IceAdapter::sendToGpgNet(GPGNetMessage const& message)
{
  if (mGPGNetServer->sessionCount() == 0)
  {
    FAF_LOG_ERROR << "sendToGpgNet failed. No sessions connected";
    return;
  }
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
    options["lobby-port"]           = mOptions.gameUdpPort;
    options["stun_ip"]              = std::string(mOptions.stunIp);
    options["turn_ip"]              = std::string(mOptions.turnIp);
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

    /*
    if (!mHostGameMap.empty())
    {
      gpgnet["host_game"]["map"] = mHostGameMap;
    }
    else if(!mJoinGameRemotePlayerLogin.empty())
    {
      gpgnet["join_game"]["remote_player_login"] = mJoinGameRemotePlayerLogin;
      gpgnet["join_game"]["remote_player_id"] = mJoinGameRemotePlayerId;
    }
    */
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

      if (it->second->iceStream())
      {
        relay["ice_agent"]["state"] = stateToString(it->second->iceStream()->state());
        relay["ice_agent"]["peer_connected_to_me"] = it->second->iceStream()->peerConnectedToMe();
        relay["ice_agent"]["connected_to_peer"] = it->second->iceStream()->connectedToPeer();
        relay["ice_agent"]["local_candidate"] = it->second->iceStream()->localCandidateInfo();
        relay["ice_agent"]["remote_candidate"] = it->second->iceStream()->remoteCandidateInfo();
        relay["ice_agent"]["remote_sdp"] = it->second->iceStream()->remoteSdp();
        relay["ice_agent"]["time_to_connected"] = it->second->iceStream()->timeToConnected();
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
      if (mGPGNetGameState == "Idle")
      {
        mGPGNetServer->sendCreateLobby(InitMode::NormalLobby,
                                       mOptions.gameUdpPort,
                                       mOptions.localPlayerLogin,
                                       mOptions.localPlayerId,
                                       1);
      }
      tryExecuteGameTasks();
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

void IceAdapter::onGpgConnectionStateChanged(TcpSession* session, ConnectionState cs)
{
  if (mRpcServer->sessionCount() > 1)
  {
    FAF_LOG_ERROR << "only 1 game session supported!!";
  }
  Json::Value params(Json::arrayValue);
  params.append(cs == ConnectionState::Connected ? "Connected" : "Disconnected");
  mRpcServer->sendRequest("onConnectionStateChanged",
                          params);
  if (cs == ConnectionState::Disconnected)
  {
    FAF_LOG_INFO << "game disconnected";
    mRelays.clear();
    mAgent->clear();
    mGPGNetGameState = "";
  }
  else
  {
    FAF_LOG_INFO << "game connected";
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
                    paramsArray[1].asInt());
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

  mRpcServer->setRpcCallback("addSdp",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             Socket* session)
  {
    if (paramsArray.size() < 2)
    {
      error = "Need 2 parameters: remotePlayerId (int), sdp (string)";
      return;
    }
    try
    {
      addSdp(paramsArray[0].asInt(),
             paramsArray[1].asString());
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
}

void IceAdapter::queueGameTask(IceAdapterGameTask t)
{
  mGameTasks.push(t);
  tryExecuteGameTasks();
}

void IceAdapter::tryExecuteGameTasks()
{
  if (mGPGNetServer->sessionCount() == 0)
  {
    return;
  }
  while (!mGameTasks.empty())
  {
    auto task = mGameTasks.front();
    switch (task.task)
    {
      case IceAdapterGameTask::JoinGame:
      {
        if (mGPGNetGameState != "Lobby")
        {
          return;
        }
        auto relayIt = mRelays.find(task.remoteId);
        if (relayIt == mRelays.end())
        {
          FAF_LOG_ERROR << "no relay found for joining player " << task.remoteId;
          return;
        }
        else
        {
          mGPGNetServer->sendJoinGame(std::string("127.0.0.1:") + std::to_string(relayIt->second->localGameUdpPort()),
                                      task.remoteLogin,
                                      task.remoteId);
        }
        break;
      }
      case IceAdapterGameTask::HostGame:
        if (mGPGNetGameState != "Lobby")
        {
          return;
        }
        mGPGNetServer->sendHostGame(task.hostMap);
        break;
      case IceAdapterGameTask::ConnectToPeer:
      {
        auto relayIt = mRelays.find(task.remoteId);
        if (relayIt == mRelays.end())
        {
          FAF_LOG_ERROR << "no relay found for joining player " << task.remoteId;
        }
        else
        {
          mGPGNetServer->sendConnectToPeer(std::string("127.0.0.1:") + std::to_string(relayIt->second->localGameUdpPort()),
                                           task.remoteLogin,
                                           task.remoteId);
        }
        break;
      }
      case IceAdapterGameTask::DisconnectFromPeer:
        mGPGNetServer->sendDisconnectFromPeer(task.remoteId);
        break;
    }
    mGameTasks.pop();
  }
}

std::shared_ptr<PeerRelay> IceAdapter::createPeerRelay(int remotePlayerId,
                                                       std::string const& remotePlayerLogin)
{
  if (!mAgent)
  {
    FAF_LOG_ERROR << "no agent created. looking up STUN/TURN servers likely failed. check internet";
    return std::shared_ptr<PeerRelay>();
  }

  auto sdpMsgCb = [this](IceStream* stream, std::string const& sdp)
  {
    Json::Value gatheredSdpParams(Json::arrayValue);
    gatheredSdpParams.append(mOptions.localPlayerId);
    gatheredSdpParams.append(stream->peerId());
    gatheredSdpParams.append(sdp);
    mRpcServer->sendRequest("onSdp",
                            gatheredSdpParams);
  };

  auto recvCb = [this](IceStream* stream, std::string const& msg)
  {
    auto relayIt = mRelays.find(stream->peerId());
    if (relayIt != mRelays.end())
    {
      relayIt->second->sendPeerMessageToGame(msg);
    }
  };

  auto stateCb = [this](IceStream* stream, IceStreamState const& state)
  {
    Json::Value iceStateParams(Json::arrayValue);
    iceStateParams.append(mOptions.localPlayerId);
    iceStateParams.append(stream->peerId());
    iceStateParams.append(stateToString(state));
    mRpcServer->sendRequest("onPeerStateChanged",
                            iceStateParams);
  };

  auto candSelectedCb = [this](IceStream* stream, std::string const& local, std::string const& remote)
  {
    Json::Value iceCandParams(Json::arrayValue);
    iceCandParams.append(mOptions.localPlayerId);
    iceCandParams.append(stream->peerId());
    iceCandParams.append(local);
    iceCandParams.append(remote);
    mRpcServer->sendRequest("onCandidateSelected",
                          iceCandParams);
  };

  auto connChangedCb = [this, remotePlayerId](IceStream* relay, bool connectedToPeer, bool peerConnectedToMe)
  {
    Json::Value onConnectedToPeerParams(Json::arrayValue);
    onConnectedToPeerParams.append(mOptions.localPlayerId);
    onConnectedToPeerParams.append(remotePlayerId);
    onConnectedToPeerParams.append(connectedToPeer);
    onConnectedToPeerParams.append(peerConnectedToMe);
    mRpcServer->sendRequest("onConnectivityChanged",
                            onConnectedToPeerParams);
  };

  auto stream = mAgent->createStream(remotePlayerId,
                                     sdpMsgCb,
                                     recvCb,
                                     stateCb,
                                     candSelectedCb,
                                     connChangedCb);

  auto result = std::make_shared<PeerRelay>(stream,
                                            remotePlayerLogin,
                                            mOptions);

  mRelays[remotePlayerId] = result;

  result->iceStream()->gatherCandidates();

  return result;
}

}
