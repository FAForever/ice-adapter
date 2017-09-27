#include "IceAdapter.h"

#include <iostream>

#include <webrtc/base/ssladapter.h>
#include <webrtc/pc/test/fakeaudiocapturemodule.h>
#include <webrtc/base/logging.h>
#include <webrtc/base/thread.h>
#include <webrtc/api/mediaconstraintsinterface.h>
#include <webrtc/api/test/fakeconstraints.h>
#include <third_party/json/json.h>

#include "logging.h"

namespace faf {

IceAdapter::IceAdapter(int argc, char *argv[]):
  _options(faf::IceAdapterOptions::init(argc, argv)),
  _gpgnetGameState("None"),
  _lobbyInitMode("normal")
{

  logging_init(_options.logLevel);

  if (!rtc::InitializeSSL())
  {
    FAF_LOG_ERROR << "Error in InitializeSSL()";
    std::exit(1);
  }

  auto audio_device_module = FakeAudioCaptureModule::Create();
_pcfactory = webrtc::CreatePeerConnectionFactory(rtc::Thread::Current(),
                                                 rtc::Thread::Current(),
                                                 audio_device_module,
                                                 nullptr,
                                                 nullptr);
  if (!_pcfactory)
  {
    FAF_LOG_ERROR << "Error in CreatePeerConnectionFactory()";
    std::exit(1);
  }

  _gpgnetServer.SignalNewGPGNetMessage.connect(this, &IceAdapter::_onGpgNetMessage);
  _gpgnetServer.SignalClientConnected.connect(this, &IceAdapter::_onGameConnected);
  _gpgnetServer.SignalClientDisconnected.connect(this, &IceAdapter::_onGameDisconnected);
  _connectRpcMethods();
  _gametaskString = "Idle";
}

IceAdapter::~IceAdapter()
{
  rtc::CleanupSSL();
}

void IceAdapter::run()
{
  _gpgnetServer.listen(_options.gpgNetPort);
  _jsonRpcServer.listen(_options.rpcPort);
  rtc::Thread::Current()->Run();
}

void IceAdapter::hostGame(std::string const& map)
{
  _queueGameTask({IceAdapterGameTask::HostGame,
                 map,
                 "",
                 0});
  _gametaskString = "Hosting map " + map + ".";
}

void IceAdapter::joinGame(std::string const& remotePlayerLogin,
                          int remotePlayerId)
{
  _createPeerRelay(remotePlayerId,
                   remotePlayerLogin,
                   false);
  _queueGameTask({IceAdapterGameTask::JoinGame,
                 "",
                 remotePlayerLogin,
                 remotePlayerId});
  _gametaskString = "Joining game from player " + remotePlayerLogin + ".";
}

void IceAdapter::connectToPeer(std::string const& remotePlayerLogin,
                               int remotePlayerId,
                               bool createOffer)
{
  _createPeerRelay(remotePlayerId,
                   remotePlayerLogin,
                   createOffer);
  _queueGameTask({IceAdapterGameTask::ConnectToPeer,
                 "",
                 remotePlayerLogin,
                 remotePlayerId});
}

void IceAdapter::disconnectFromPeer(int remotePlayerId)
{
  auto relayIt = _relays.find(remotePlayerId);
  if (relayIt == _relays.end())
  {
    FAF_LOG_TRACE << "no relay for remote peer " << remotePlayerId << " found";
    return;
  }
  _relays.erase(relayIt);
  FAF_LOG_INFO << "removed relay for peer " << remotePlayerId;
  _queueGameTask({IceAdapterGameTask::DisconnectFromPeer,
                  "",
                  "",
                  remotePlayerId});
}

void IceAdapter::setLobbyInitMode(std::string const& initMode)
{
  _lobbyInitMode = initMode;
}

void IceAdapter::iceMsg(int remotePlayerId, Json::Value const& msg)
{
  auto relayIt = _relays.find(remotePlayerId);
  if (relayIt == _relays.end())
  {
    FAF_LOG_ERROR << "no relay for remote peer " << remotePlayerId << " found";
    return;
  }

  relayIt->second->addIceMessage(msg);
}

void IceAdapter::sendToGpgNet(GPGNetMessage const& message)
{
  if (!_gpgnetServer.hasConnectedClient())
  {
    FAF_LOG_ERROR << "sendToGpgNet failed. No sessions connected";
    return;
  }
  _gpgnetServer.sendMessage(message);
}

void IceAdapter::setIceServers(Json::Value const& servers)
{
  _iceServerList.clear();
  for(std::size_t iServer = 0; iServer < servers.size(); ++iServer)
  {
    auto serverJson = servers[Json::ArrayIndex(iServer)];
    if (serverJson.isObject())
    {
      webrtc::PeerConnectionInterface::IceServer iceServer;
      iceServer.uri = serverJson["url"].asString();
      if (serverJson["urls"].isArray())
      {
        for(std::size_t iUrl = 0; iUrl < serverJson["urls"].size(); ++iUrl)
        {
          iceServer.urls.push_back(serverJson["urls"][Json::ArrayIndex(iUrl)].asString());
        }
      }
      iceServer.password = serverJson["credential"].asString();
      iceServer.username = serverJson["username"].asString();
      _iceServerList.push_back(iceServer);
    }
  }
}

Json::Value IceAdapter::status() const
{
  Json::Value result;
  result["version"] = FAF_VERSION_STRING;
  /* Options */
  {
    Json::Value options;

    options["player_id"]            = _options.localPlayerId;
    options["player_login"]         = std::string(_options.localPlayerLogin);
    options["rpc_port"]             = _options.rpcPort;
    options["gpgnet_port"]          = _options.gpgNetPort;
    options["lobby_port"]           = _options.gameUdpPort;
    options["log_file"]             = std::string(_options.logFile);
    result["options"] = options;
  }
  /* GPGNet */
  {
    Json::Value gpgnet;

    gpgnet["local_port"] = _gpgnetServer.listenPort();
    gpgnet["connected"] = _gpgnetServer.hasConnectedClient();
    gpgnet["game_state"] = _gpgnetGameState;
    gpgnet["task_string"] = _gametaskString;
    result["gpgnet"] = gpgnet;
  }
  /* Relays */
  {
    Json::Value relays(Json::arrayValue);
    for (auto it = _relays.begin(), end = _relays.end(); it != end; ++it)
    {
      Json::Value relay;
      relay["remote_player_id"] = it->first;

      /* TODO
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
      */

      relays.append(relay);
    }
    result["relays"] = relays;
  }
  return result;
}

void IceAdapter::_connectRpcMethods()
{
  _jsonRpcServer.setRpcCallback("quit",
                             [](Json::Value const& paramsArray,
                                Json::Value & result,
                                Json::Value & error,
                                rtc::AsyncSocket* session)
  {
    result = "ok";
    rtc::Thread::Current()->Quit();
  });

  _jsonRpcServer.setRpcCallback("hostGame",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
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

  _jsonRpcServer.setRpcCallback("joinGame",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
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

  _jsonRpcServer.setRpcCallback("connectToPeer",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
  {
    if (paramsArray.size() < 3)
    {
      error = "Need 3 parameters: remotePlayerLogin (string), remotePlayerId (int), createOffer (bool)";
      return;
    }
    try
    {
      connectToPeer(paramsArray[0].asString(),
                    paramsArray[1].asInt(),
                    paramsArray[2].asBool());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  _jsonRpcServer.setRpcCallback("disconnectFromPeer",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
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

  _jsonRpcServer.setRpcCallback("setLobbyInitMode",
                                [this](Json::Value const& paramsArray,
                                Json::Value & result,
                                Json::Value & error,
                                rtc::AsyncSocket* session)
  {
    if (paramsArray.size() < 1 ||
        !paramsArray[0].isString())
    {
      error = "Need 1 parameters: initMode (string)";
      return;
    }
    try
    {
      setLobbyInitMode(paramsArray[0].asString());
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  _jsonRpcServer.setRpcCallback("iceMsg",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
  {
    if (paramsArray.size() < 2 ||
        !paramsArray[1].isObject())
    {
      error = "Need 2 parameters: remotePlayerId (int), msg (object)";
      return;
    }
    try
    {
      iceMsg(paramsArray[0].asInt(),
             paramsArray[1]);
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  _jsonRpcServer.setRpcCallback("sendToGpgNet",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
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
      for(std::size_t i = 0; i < paramsArray[1].size(); ++i)
      {
        message.chunks.push_back(paramsArray[1][Json::ArrayIndex(i)]);
      }
      sendToGpgNet(message);
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }
  });

  _jsonRpcServer.setRpcCallback("setIceServers",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
  {
    if (paramsArray.size() < 1 ||
        !paramsArray[0].isArray())
    {
      error = "Need 1 parameters: iceServers (array) ";
      return;
    }
    try
    {
      setIceServers(paramsArray[0]);
      result = "ok";
    }
    catch(std::exception& e)
    {
      error = e.what();
    }

  });

  _jsonRpcServer.setRpcCallback("status",
                             [this](Json::Value const& paramsArray,
                             Json::Value & result,
                             Json::Value & error,
                             rtc::AsyncSocket* session)
  {
    result = status();
  });
}

void IceAdapter::_queueGameTask(IceAdapterGameTask t)
{
  _gameTasks.push(t);
  _tryExecuteGameTasks();
}

void IceAdapter::_tryExecuteGameTasks()
{
  if (!_gpgnetServer.hasConnectedClient())
  {
    return;
  }
  while (!_gameTasks.empty())
  {
    auto task = _gameTasks.front();
    switch (task.task)
    {
      case IceAdapterGameTask::HostGame:
        if (_gpgnetGameState != "Lobby")
        {
          return;
        }
        _gpgnetServer.sendHostGame(task.hostMap);
        break;
      case IceAdapterGameTask::JoinGame:
      case IceAdapterGameTask::ConnectToPeer:
      {
        if (_gpgnetGameState != "Lobby")
        {
          return;
        }
        auto relayIt = _relays.find(task.remoteId);
        if (relayIt == _relays.end())
        {
          FAF_LOG_ERROR << "no relay found for joining player " << task.remoteId;
        }
        else
        {
          _gpgnetServer.sendConnectToPeer(std::string("127.0.0.1:") + std::to_string(relayIt->second->localUdpSocketPort()),
                                           task.remoteLogin,
                                           task.remoteId);
        }
        break;
      }
      case IceAdapterGameTask::DisconnectFromPeer:
        _gpgnetServer.sendDisconnectFromPeer(task.remoteId);
        break;
    }
    _gameTasks.pop();
  }
}

void IceAdapter::_onGameConnected()
{
  FAF_LOG_INFO << "game connected";
  _jsonRpcServer.sendRequest("onConnectionStateChanged",
                             {"Connected"});
}

void IceAdapter::_onGameDisconnected()
{
  FAF_LOG_INFO << "game disconnected";
  _jsonRpcServer.sendRequest("onConnectionStateChanged",
                             {"Disconnected"});
  _gametaskString = "Idle";
  _gpgnetGameState = "None";
  _relays.clear();
}

void IceAdapter::_onGpgNetMessage(GPGNetMessage const& message)
{
  if (message.header == "GameState")
  {
    if (message.chunks.size() == 1)
    {
      _gpgnetGameState = message.chunks[0].asString();
      if (_gpgnetGameState == "Idle")
      {
        _gpgnetServer.sendCreateLobby(InitMode::NormalLobby,
                                      _options.gameUdpPort,
                                      _options.localPlayerLogin,
                                      _options.localPlayerId,
                                      1);
      }
      _tryExecuteGameTasks();
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
  _jsonRpcServer.sendRequest("onGpgNetMessageReceived",
                             rpcParams);
}

std::shared_ptr<PeerRelay> IceAdapter::_createPeerRelay(int remotePlayerId,
                                                        std::string const& remotePlayerLogin,
                                                        bool createOffer)
{
/* TODO
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
*/

  webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  configuration.servers = _iceServerList;

  webrtc::FakeConstraints constraints;
  constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, webrtc::MediaConstraintsInterface::kValueTrue);
  // FIXME: crashes without these constraints, why?
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveAudio, webrtc::MediaConstraintsInterface::kValueFalse);
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo, webrtc::MediaConstraintsInterface::kValueFalse);

  auto relay = std::make_shared<PeerRelay>(remotePlayerId,
                                           remotePlayerLogin,
                                           createOffer,
                                           _options.gameUdpPort);
  auto observer = std::make_shared<PeerConnectionObserver>(relay.get());
  auto peerConnection = _pcfactory->CreatePeerConnection(configuration,
                                                         &constraints,
                                                         nullptr,
                                                         nullptr,
                                                         observer.get());

  relay->setIceMessageCallback([this, remotePlayerId](Json::Value const& iceMsg)
  {
    Json::Value onIceMsgParams(Json::arrayValue);
    onIceMsgParams.append(_options.localPlayerId);
    onIceMsgParams.append(remotePlayerId);
    onIceMsgParams.append(iceMsg);
    _jsonRpcServer.sendRequest("onIceMsg",
                               onIceMsgParams);
  });

  relay->setStateCallback([this, remotePlayerId](std::string const& state)
  {
    Json::Value onIceStateChangedParams(Json::arrayValue);
    onIceStateChangedParams.append(_options.localPlayerId);
    onIceStateChangedParams.append(remotePlayerId);
    onIceStateChangedParams.append(state);
    _jsonRpcServer.sendRequest("onIceConnectionStateChanged",
                               onIceStateChangedParams);
  });

  relay->setDataChannelOpenCallback([this, remotePlayerId]()
  {
    Json::Value onDatachannelOpenParams(Json::arrayValue);
    onDatachannelOpenParams.append(_options.localPlayerId);
    onDatachannelOpenParams.append(remotePlayerId);
    _jsonRpcServer.sendRequest("onDatachannelOpen",
                               onDatachannelOpenParams);
  });

  relay->setConnection(peerConnection,
                       observer);
  _relays[remotePlayerId] = relay;
  return relay;
}

} // namespace faf
