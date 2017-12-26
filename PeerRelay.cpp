#include "PeerRelay.h"

#include "logging.h"
#include "PeerRelayObservers.h"

namespace faf {

#define RELAY_LOG_ERROR FAF_LOG_ERROR << "PeerRelay for " << _remotePlayerLogin << " (" << _remotePlayerId << "): "
#define RELAY_LOG_WARN FAF_LOG_WARN << "PeerRelay for " << _remotePlayerLogin << " (" << _remotePlayerId << "): "
#define RELAY_LOG_INFO FAF_LOG_INFO << "PeerRelay for " << _remotePlayerLogin << " (" << _remotePlayerId << "): "
#define RELAY_LOG_DEBUG FAF_LOG_DEBUG << "PeerRelay for " << _remotePlayerLogin << " (" << _remotePlayerId << "): "
#define RELAY_LOG_TRACE FAF_LOG_TRACE << "PeerRelay for " << _remotePlayerLogin << " (" << _remotePlayerId << "): "

PeerRelay::PeerRelay(Options options,
                     Callbacks callbacks,
                     rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> const& pcfactory):
  _pcfactory(pcfactory),
  _iceServerList(options.iceServers),
  _createOfferObserver(new rtc::RefCountedObject<CreateOfferObserver>(this)),
  _createAnswerObserver(new rtc::RefCountedObject<CreateAnswerObserver>(this)),
  _setLocalDescriptionObserver(new rtc::RefCountedObject<SetLocalDescriptionObserver>(this)),
  _setRemoteDescriptionObserver(new rtc::RefCountedObject<SetRemoteDescriptionObserver>(this)),
  _rtcStatsCollectorCallback(new rtc::RefCountedObject<RTCStatsCollectorCallback>(this)),
  _dataChannelObserver(std::make_unique<DataChannelObserver>(this)),
  _peerConnectionObserver(std::make_shared<PeerConnectionObserver>(this)),
  _remotePlayerId(options.remotePlayerId),
  _remotePlayerLogin(options.remotePlayerLogin),
  _isOfferer(options.isOfferer),
  _gameUdpAddress("127.0.0.1", options.gameUdpPort),
  _localUdpSocket(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(AF_INET, SOCK_DGRAM)),
  _callbacks(callbacks),
  _isConnected(false),
  _closing(false),
  _iceState("none"),
  _connectionAttemptTimeout(std::chrono::seconds(10))
{
  _localUdpSocket->SignalReadEvent.connect(this, &PeerRelay::_onPeerdataFromGame);
  if (_localUdpSocket->Bind(rtc::SocketAddress("127.0.0.1", 0)) != 0)
  {
    RELAY_LOG_ERROR << "unable to bind local udp socket";
  }
  _localUdpSocketPort = _localUdpSocket->GetLocalAddress().port();
  RELAY_LOG_INFO << "listening on UDP port " << _localUdpSocketPort;

  _connectStartTime = std::chrono::steady_clock::now();

  webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  configuration.servers = _iceServerList;
  _peerConnection = _pcfactory->CreatePeerConnection(configuration,
                                                     nullptr,
                                                     nullptr,
                                                     _peerConnectionObserver.get());
  _createOffer();
}

PeerRelay::~PeerRelay()
{
  _closing = true;
  if (_dataChannel)
  {
    _dataChannel->UnregisterObserver();
    _dataChannel.release();
  }
  if (_peerConnection)
  {
    _peerConnection->Close();
    _peerConnection.release();
  }
}

int PeerRelay::localUdpSocketPort() const
{
  return _localUdpSocketPort;
}

Json::Value PeerRelay::status() const
{
  Json::Value result;
  result["remote_player_id"] = _remotePlayerId;
  result["remote_player_login"] = _remotePlayerLogin;
  result["local_game_udp_port"] = _localUdpSocketPort;
  result["local_game_udp_port"] = _localUdpSocketPort;
  result["ice_agent"] = Json::Value();
  result["ice_agent"]["state"] = _iceState;
  result["ice_agent"]["connected"] = _isConnected;
  result["ice_agent"]["loc_cand_addr"] = _localCandAddress;
  result["ice_agent"]["rem_cand_addr"] = _remoteCandAddress;
  result["ice_agent"]["loc_cand_type"] = _localCandType;
  result["ice_agent"]["rem_cand_type"] = _remoteCandType;
  result["ice_agent"]["time_to_connected"] = _isConnected ? std::chrono::duration_cast<std::chrono::milliseconds>(_connectDuration).count() / 1000. : 0.;
  return result;
}

bool PeerRelay::isConnected() const
{
  return _isConnected;
}

void PeerRelay::setIceServers(webrtc::PeerConnectionInterface::IceServers const& iceServers)
{
  _iceServerList = iceServers;
}

void PeerRelay::addIceMessage(Json::Value const& iceMsg)
{
  FAF_LOG_DEBUG << "addIceMessage: " << Json::FastWriter().write(iceMsg);
  if (!_peerConnection)
  {
    FAF_LOG_ERROR << "!_peerConnection";
    return;
  }
  if (iceMsg["type"].asString() == "offer" ||
      iceMsg["type"].asString() == "answer")
  {
    webrtc::SdpParseError error;
    auto sdp = webrtc::CreateSessionDescription(iceMsg["type"].asString(), iceMsg["sdp"].asString(), &error);
    if (sdp)
    {
      _peerConnection->SetRemoteDescription(_setRemoteDescriptionObserver, sdp);
    }
    else
    {
      FAF_LOG_ERROR << "parsing remote SDP failed: " << error.description;
    }
  }
  else if (iceMsg["type"].asString() == "candidate")
  {
    webrtc::SdpParseError error;
    auto candidate = webrtc::CreateIceCandidate(iceMsg["candidate"]["sdpMid"].asString(),
                                                iceMsg["candidate"]["sdpMLineIndex"].asInt(),
                                                iceMsg["candidate"]["candidate"].asString(),
                                                &error);
    if (!candidate)
    {
      FAF_LOG_ERROR << "parsing ICE candidate failed: " << error.description;
    }
    else if (!_peerConnection->AddIceCandidate(candidate))
    {
      FAF_LOG_ERROR << "adding ICE candidate failed";
    };
    delete candidate;
  }
}

void PeerRelay::_createOffer()
{
  bool reconnect = _dataChannel;
  if (_isOfferer)
  {
    if (!_dataChannel)
    {
      webrtc::DataChannelInit dataChannelInit;
      dataChannelInit.ordered = false;
      dataChannelInit.maxRetransmits = 0;
      _dataChannel = _peerConnection->CreateDataChannel("faf",
                                                        &dataChannelInit);
      _dataChannel->RegisterObserver(_dataChannelObserver.get());
    }
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = 0;
    options.offer_to_receive_video = 0;
    options.ice_restart = reconnect;
    _peerConnection->CreateOffer(_createOfferObserver,
                                 options);
    _restartOfferTimer.start(3000, [this]
    {
      if (!isConnected())
      {
        _createOffer();
      }
    });
  }
}

void PeerRelay::_setIceState(std::string const& state)
{
  RELAY_LOG_DEBUG << "ice state changed to" << state;
  _iceState = state;
  if (_closing)
  {
    return;
  }

  if (_iceState == "connected" ||
      _iceState == "completed")
  {
    _setConnected(true);
  }
  else
  {
    _setConnected(false);
  }

  if (!_closing &&
      _peerConnection)
  {
    _peerConnection->GetStats(_rtcStatsCollectorCallback.get());
  }

  if (_callbacks.stateCallback)
  {
    _callbacks.stateCallback(_iceState);
  }

  if (_isOfferer)
  {
    if (_iceState == "failed" ||
        _iceState == "disconnected" ||
        _iceState == "closed")

    {
      RELAY_LOG_WARN << "Connection lost, forcing reconnect immediately.";
      _createOffer();
    }
  }
}

void PeerRelay::_setConnected(bool connected)
{
  if (connected != _isConnected)
  {
    _isConnected = connected;
    if (_callbacks.connectedCallback)
    {
      _callbacks.connectedCallback(true);
    }
    if (connected)
    {
      _connectDuration = std::chrono::steady_clock::now() - _connectStartTime;
      RELAY_LOG_INFO << "connected after " <<  std::chrono::duration_cast<std::chrono::milliseconds>(_connectDuration).count() / 1000.;
    }
    else
    {
      RELAY_LOG_INFO << "disconnected";
    }
  }
}

void PeerRelay::_onPeerdataFromGame(rtc::AsyncSocket* socket)
{
  auto msgLength = socket->Recv(_readBuffer.data(), _readBuffer.size(), nullptr);
  if (!_isConnected)
  {
    RELAY_LOG_TRACE << "skipping " << msgLength << " bytes of P2P data until ICE connection is established";
    return;
  }
  if (msgLength > 0 && _dataChannel)
  {
    _dataChannel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(_readBuffer.data(), static_cast<std::size_t>(msgLength)), true));
  }
}

} // namespace faf
