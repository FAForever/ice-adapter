#include "PeerRelay.h"

#include <webrtc/base/bind.h>

#include "logging.h"
#include "PeerRelayObservers.h"

namespace faf {

#define RELAY_LOG(x) LOG(x) << "PeerRelay for " << _remotePlayerLogin << " (" << _remotePlayerId << "): "

PeerRelay::PeerRelay(int remotePlayerId,
                     std::string const& remotePlayerLogin,
                     bool createOffer,
                     int gameUdpPort,
                     rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> const& pcfactory):
  _pcfactory(pcfactory),
  _localSdp(nullptr),
  _createOfferObserver(new rtc::RefCountedObject<CreateOfferObserver>(this)),
  _createAnswerObserver(new rtc::RefCountedObject<CreateAnswerObserver>(this)),
  _setLocalDescriptionObserver(new rtc::RefCountedObject<SetLocalDescriptionObserver>(this)),
  _setRemoteDescriptionObserver(new rtc::RefCountedObject<SetRemoteDescriptionObserver>(this)),
  _rtcStatsCollectorCallback(new rtc::RefCountedObject<RTCStatsCollectorCallback>(this)),
  _dataChannelObserver(std::make_unique<DataChannelObserver>(this)),
  _peerConnectionObserver(std::make_shared<PeerConnectionObserver>(this)),
  _remotePlayerId(remotePlayerId),
  _remotePlayerLogin(remotePlayerLogin),
  _createOffer(createOffer),
  _localUdpSocket(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_DGRAM)),
  _gameUdpAddress("127.0.0.1", gameUdpPort),
  _receivedOffer(false),
  _isConnected(false),
  _iceState("none"),
  _queue("Relay", rtc::TaskQueue::Priority::LOW),
  _connectionAttemptTimeoutMs(std::chrono::seconds(10))
{
  _localUdpSocket->SignalReadEvent.connect(this, &PeerRelay::_onPeerdataFromGame);
  if (_localUdpSocket->Bind(rtc::SocketAddress("127.0.0.1", 0)) != 0)
  {
    FAF_LOG_ERROR << "unable to bind local udp socket";
  }
  _localUdpSocketPort = _localUdpSocket->GetLocalAddress().port();
  FAF_LOG_INFO << "PeerRelay for " << remotePlayerLogin << " (" << remotePlayerId << ") listening on UDP port " << _localUdpSocketPort;
}

PeerRelay::~PeerRelay()
{
  if (_dataChannel)
  {
    _dataChannel->UnregisterObserver();
  }
  if (_peerConnection)
  {
    _peerConnection->Close();
  }
}

void PeerRelay::reinit()
{
  _queue.PostDelayedTask(rtc::Bind(&PeerRelay::_checkConnectionTimeout, this), 1000);
  _connectStartTime = std::chrono::steady_clock::now();
  _setConnected(false);
  _receivedOffer = false;
  _dataChannel.release();
  if (_peerConnection)
  {
    _peerConnection->Close();
  }
  if (_stateCallback)
  {
    _stateCallback("none");
  }

  webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  configuration.servers = _iceServerList;
  _peerConnection = _pcfactory->CreatePeerConnection(configuration,
                                                     nullptr,
                                                     nullptr,
                                                     _peerConnectionObserver.get());

  if (_createOffer)
  {
    webrtc::DataChannelInit dataChannelInit;
    dataChannelInit.maxRetransmits = 0;
    dataChannelInit.ordered = false;
    _dataChannel = _peerConnection->CreateDataChannel("faf",
                                                      &dataChannelInit);
    _dataChannel->RegisterObserver(_dataChannelObserver.get());
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = 0;
    options.offer_to_receive_video = 0;
    _peerConnection->CreateOffer(_createOfferObserver,
                                 options);
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

void PeerRelay::setIceMessageCallback(IceMessageCallback cb)
{
  _iceMessageCallback = cb;
}

void PeerRelay::setStateCallback(StateCallback cb)
{
  _stateCallback = cb;
}

void PeerRelay::setConnectedCallback(ConnectedCallback cb)
{
  _connectedCallback = cb;
}

void PeerRelay::setIceServers(webrtc::PeerConnectionInterface::IceServers const& iceServers)
{
  _iceServerList = iceServers;
}

void PeerRelay::addIceMessage(Json::Value const& iceMsg)
{
  if (!_peerConnection)
  {
    return;
  }
  if (iceMsg["type"].asString() == "offer" ||
      iceMsg["type"].asString() == "answer")
  {
    webrtc::SdpParseError error;
    _receivedOffer = iceMsg["type"].asString() == "offer";
    _peerConnection->SetRemoteDescription(_setRemoteDescriptionObserver,
                                      webrtc::CreateSessionDescription(iceMsg["type"].asString(),
                                                                       iceMsg["sdp"].asString(),
                                                                       &error));
  }
  else if (iceMsg["type"].asString() == "candidate")
  {
    webrtc::SdpParseError error;
    _peerConnection->AddIceCandidate(webrtc::CreateIceCandidate(iceMsg["candidate"]["sdpMid"].asString(),
                                                            iceMsg["candidate"]["sdpMLineIndex"].asInt(),
                                                            iceMsg["candidate"]["candidate"].asString(),
                                                            &error));
  }
}

void PeerRelay::_setIceState(std::string const& state)
{
  RELAY_LOG(LS_VERBOSE) << "ice state changed to" << state;
  _iceState = state;
  if (_iceState == "connected" ||
      _iceState == "completed")
  {
    _setConnected(true);
  }
  if (_stateCallback)
  {
    _stateCallback(_iceState);
  }
  if (_iceState == "disconnected" ||
      _iceState == "failed" ||
      _iceState == "closed")
  {
    _setConnected(false);
  }
  if (_iceState == "failed")
  {
    RELAY_LOG(LS_WARNING) << "Connection failed, forcing reconnect immediately.";
    reinit();
  }
}

void PeerRelay::_setConnected(bool connected)
{
  if (connected != _isConnected)
  {
    _isConnected = connected;
    if (_connectedCallback)
    {
      _connectedCallback(true);
    }
    if (connected)
    {
      _connectDuration = std::chrono::steady_clock::now() - _connectStartTime;
      RELAY_LOG(LS_INFO) << "connected after " <<  std::chrono::duration_cast<std::chrono::milliseconds>(_connectDuration).count() / 1000.;
      _peerConnection->GetStats(_rtcStatsCollectorCallback.get());
    }
    else
    {
      RELAY_LOG(LS_INFO) << "disconnected";
    }
  }
}

void PeerRelay::_checkConnectionTimeout()
{
  if (_iceState == "new" ||
      _iceState == "closed" ||
      _iceState == "failed")
  {
    auto timeSinceLastConnectionAttempt = std::chrono::steady_clock::now() - _connectStartTime;
    if (timeSinceLastConnectionAttempt > _connectionAttemptTimeoutMs)
    {
      if (_createOffer)
      {
        RELAY_LOG(LS_WARNING) << "ICE connection state is stuck in offerer. Forcing reconnect...";
      }
      else if (_receivedOffer)
      {
        RELAY_LOG(LS_WARNING) << "ICE connection state is stuck in answerer. Forcing reconnect...";
      }
      reinit();
      return;
    }
  }
  _queue.PostDelayedTask(rtc::Bind(&PeerRelay::_checkConnectionTimeout, this), 1000);
}

void PeerRelay::_onPeerdataFromGame(rtc::AsyncSocket* socket)
{
  auto msgLength = socket->Recv(_readBuffer.data(), _readBuffer.size(), nullptr);
  if (!_isConnected)
  {
    FAF_LOG_TRACE << "skipping " << msgLength << " bytes of P2P data until ICE connection is established";
    return;
  }
  if (msgLength > 0 && _dataChannel)
  {
    _dataChannel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(_readBuffer.data(), static_cast<std::size_t>(msgLength)), true));
  }
}

} // namespace faf
