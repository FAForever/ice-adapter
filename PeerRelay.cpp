#include "PeerRelay.h"

#include <algorithm>

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
  _callbacks(callbacks)
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
  configuration.ice_check_min_interval = 5000;
  _peerConnection = _pcfactory->CreatePeerConnection(configuration,
                                                     nullptr,
                                                     nullptr,
                                                     _peerConnectionObserver.get());
  if (!_peerConnection)
  {
    FAF_LOG_ERROR << "_pcfactory->CreatePeerConnection() failed!";
    std::exit(1);
  }

  if (_isOfferer)
  {
    _createOffer();
  }
}

PeerRelay::~PeerRelay()
{
  _closing = true;
  if (_dataChannel)
  {
    _dataChannel->UnregisterObserver();
    _dataChannel->Close();
    _dataChannel = nullptr;
  }
  _peerConnection->Close();
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
  result["ice"] = Json::Value();
  result["ice"]["offerer"] = _isOfferer;
  result["ice"]["state"] = _iceState;
  result["ice"]["gathering_state"] = _iceGatheringState;
  result["ice"]["datachannel_state"] = _dataChannelState;
  result["ice"]["connected"] = _isConnected;
  result["ice"]["loc_cand_addr"] = _localCandAddress;
  result["ice"]["rem_cand_addr"] = _remoteCandAddress;
  result["ice"]["loc_cand_type"] = _localCandType;
  result["ice"]["rem_cand_type"] = _remoteCandType;
  result["ice"]["time_to_connected"] = _isConnected ? std::chrono::duration_cast<std::chrono::milliseconds>(_connectDuration).count() / 1000. : 0.;
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
  if (_isOfferer)
  {
    RELAY_LOG_DEBUG << "creating offer";
    bool reconnect = true;
    if (!_dataChannel)
    {
      reconnect = false;
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

    /* this is a terrible hack to not call the PeerConnectivityChecker destructor in its own callback */
    _connectionChecker = std::make_unique<PeerConnectivityChecker>(_dataChannel,
                                                                   [this]()
    {
      _createNewOfferTimer.singleShot(1, [this]()
      {
        _createOffer();
      });
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
      _callbacks.connectedCallback(connected);
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
  if (connected)
  {
    _peerConnection->GetStats(_rtcStatsCollectorCallback.get());
  }
}

void PeerRelay::_onPeerdataFromGame(rtc::AsyncSocket* socket)
{
  _sendCowBuffer.EnsureCapacity(sendBufferSize);
  auto msgLength = socket->Recv(_sendCowBuffer.data(), sendBufferSize, nullptr);

  if (!_isConnected)
  {
    RELAY_LOG_TRACE << "skipping " << msgLength << " bytes of P2P data until ICE connection is established";
    return;
  }
  if (msgLength > 0 && _dataChannel)
  {
    /* I hope the buffer doesn't shrink upon SetSize() */
    _sendCowBuffer.SetSize(msgLength);
    _dataChannel->Send({_sendCowBuffer, true});
  }
}

void PeerRelay::_onRemoteMessage(const uint8_t* data, std::size_t size)
{
  if (_connectionChecker &&
      _connectionChecker->handleMessageFromPeer(data, size))
  {
    return;
  }
  if (!_isOfferer &&
      size == sizeof(PeerConnectivityChecker::PingMessage) &&
      std::equal(data,
                 data + sizeof(PeerConnectivityChecker::PingMessage),
                 PeerConnectivityChecker::PingMessage) &&
      _dataChannel)
  {
    _dataChannel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(PeerConnectivityChecker::PongMessage, sizeof(PeerConnectivityChecker::PongMessage)), true));
    return;
  }
  _localUdpSocket->SendTo(data,
                          size,
                          _gameUdpAddress);
}


} // namespace faf
