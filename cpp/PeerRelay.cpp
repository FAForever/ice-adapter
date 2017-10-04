#include "PeerRelay.h"

#include <webrtc/api/mediaconstraintsinterface.h>
#include <webrtc/api/test/fakeconstraints.h>
#include <webrtc/base/bind.h>
#include <webrtc/api/stats/rtcstats_objects.h>

#include "logging.h"

namespace faf {


#define RELAY_LOG(x) LOG(x) << "PeerRelay for " << _remotePlayerLogin << " (" << _remotePlayerId << "): "
#define OBSERVER_LOG(x) LOG(x) << "PeerRelay for " << _relay->_remotePlayerLogin << " (" << _relay->_remotePlayerId << "): "

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface *sdp)
{
  //FAF_LOG_DEBUG << "CreateOfferObserver::OnSuccess";
  if (_relay->_connection)
  {
    _relay->_localSdp = sdp;
    _relay->_connection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
                                             sdp);
  }
}

void CreateOfferObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG(LS_WARNING) << "CreateOfferObserver::OnFailure: " << msg;
}

void CreateAnswerObserver::OnSuccess(webrtc::SessionDescriptionInterface *sdp)
{
  //FAF_LOG_DEBUG << "CreateAnswerObserver::OnSuccess";
  if (_relay->_connection)
  {
    _relay->_localSdp = sdp;
    _relay->_connection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
                                             sdp);
  }
}

void CreateAnswerObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG(LS_WARNING) << "CreateAnswerObserver::OnFailure: " << msg;
  //FAF_LOG_DEBUG << "CreateAnswerObserver::OnFailure: " << msg;
}

void SetLocalDescriptionObserver::OnSuccess()
{
  //FAF_LOG_DEBUG << "SetLocalDescriptionObserver::OnSuccess";
  if (_relay->_localSdp &&
      _relay->_iceMessageCallback)
  {
    Json::Value iceMsg;
    std::string sdpString;
    _relay->_localSdp->ToString(&sdpString);
    iceMsg["type"] = _relay->_createOffer ? "offer" : "answer";
    iceMsg["sdp"] = sdpString;
    _relay->_iceMessageCallback(iceMsg);
  }
}

void SetLocalDescriptionObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG(LS_WARNING) << "SetLocalDescriptionObserver::OnFailure: " << msg;
  //FAF_LOG_DEBUG << "SetLocalDescriptionObserver::OnFailure";
}

void SetRemoteDescriptionObserver::OnSuccess()
{
  //FAF_LOG_DEBUG << "SetRemoteDescriptionObserver::OnSuccess";
  if (_relay->_connection &&
      !_relay->_createOffer)
  {
    _relay->_connection->CreateAnswer(_relay->_createAnswerObserver,
                                      nullptr);
  }
}

void SetRemoteDescriptionObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG(LS_WARNING) << "SetRemoteDescriptionObserver::OnFailure: " << msg;
  //FAF_LOG_DEBUG << "SetRemoteDescriptionObserver::OnFailure";
}

void PeerConnectionObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
  //FAF_LOG_DEBUG << "PeerConnectionObserver::OnSignalingChange";
}

void PeerConnectionObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
  switch (new_state)
  {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      _relay->_setIceState("new");
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      _relay->_setIceState("checking");
    break;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      _relay->_setIceState("connected");
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      _relay->_setIceState("completed");
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      _relay->_setIceState("failed");
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      _relay->_setIceState("disconnected");
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      _relay->_setIceState("closed");
      break;
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
      /* not in https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState */
      break;
  }
}

void PeerConnectionObserver::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state)
{
  FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceGatheringChange";
}

void PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
  FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceCandidate";

  if (_relay->_iceMessageCallback)
  {
    Json::Value candidateJson;
    std::string candidateString;
    candidate->ToString(&candidateString);
    candidateJson["candidate"] = candidateString;
    candidateJson["sdpMid"] = candidate->sdp_mid();
    candidateJson["sdpMLineIndex"] = candidate->sdp_mline_index();
    Json::Value iceMsg;
    iceMsg["type"] = "candidate";
    iceMsg["candidate"] = candidateJson;
    _relay->_iceMessageCallback(iceMsg);
  }
}

void PeerConnectionObserver::OnRenegotiationNeeded()
{
  FAF_LOG_DEBUG << "PeerConnectionObserver::OnRenegotiationNeeded";
}

void PeerConnectionObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{
  FAF_LOG_DEBUG << "PeerConnectionObserver::OnDataChannel";
  _relay->_dataChannel = data_channel;
  _relay->_dataChannel->RegisterObserver(_relay->_dataChannelObserver.get());
}

void PeerConnectionObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
  FAF_LOG_DEBUG << "PeerConnectionObserver::OnAddStream";
}

void PeerConnectionObserver::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
  FAF_LOG_DEBUG << "PeerConnectionObserver::OnRemoveStream";
}

void DataChannelObserver::OnStateChange()
{
  if (_relay->_dataChannel)
  {
    switch(_relay->_dataChannel->state())
    {
      case webrtc::DataChannelInterface::kConnecting:
        //FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Connecting";
        break;
      case webrtc::DataChannelInterface::kOpen:
        //FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Open";
        _relay->_dataChannelIsOpen = true;
        _relay->_setConnected(true);
        break;
      case webrtc::DataChannelInterface::kClosing:
        //FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closing";
        _relay->_dataChannelIsOpen = false;
        break;
      case webrtc::DataChannelInterface::kClosed:
        //FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closed";
        _relay->_dataChannelIsOpen = false;
        _relay->_setConnected(false);
        break;
    }
  }
}
void DataChannelObserver::OnMessage(const webrtc::DataBuffer& buffer)
{
  FAF_LOG_DEBUG << "DataChannelObserver::OnMessage";
  if (_relay->_localUdpSocket)
  {
    _relay->_localUdpSocket->SendTo(buffer.data.cdata(),
                                    buffer.data.size(),
                                    _relay->_gameUdpAddress);
  }
}

void RTCStatsCollectorCallback::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{
  if (!report)
  {
    return;
  }
  std::string localCandId;
  std::string remoteCandId;
  auto pairs = report->GetStatsOfType<webrtc::RTCIceCandidatePairStats>();
  for (auto pair: pairs)
  {
    if (*pair->state == "succeeded")
    {
      localCandId = *pair->local_candidate_id;
      remoteCandId = *pair->remote_candidate_id;
      break;
    }
  }
  if (localCandId.empty())
  {
    return;
  }
  auto lCand = static_cast<webrtc::RTCLocalIceCandidateStats const*>(report->Get(localCandId));
  if (lCand)
  {
    _relay->_localCandAddress = *lCand->protocol + " " + *lCand->ip +":" + std::to_string(*lCand->port);
    _relay->_localCandType = *lCand->candidate_type;
  }
  auto rCand = static_cast<webrtc::RTCRemoteIceCandidateStats const*>(report->Get(remoteCandId));
  if (rCand)
  {
    _relay->_remoteCandAddress = *rCand->protocol + " " + *rCand->ip +":" + std::to_string(*rCand->port);
    _relay->_remoteCandType = *rCand->candidate_type;
  }
}

PeerRelay::PeerRelay(int remotePlayerId,
                     std::string const& remotePlayerLogin,
                     bool createOffer,
                     int gameUdpPort,
                     rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> const& pcfactory):
  _pcfactory(pcfactory),
  _createOfferObserver(new rtc::RefCountedObject<CreateOfferObserver>(this)),
  _createAnswerObserver(new rtc::RefCountedObject<CreateAnswerObserver>(this)),
  _setLocalDescriptionObserver(new rtc::RefCountedObject<SetLocalDescriptionObserver>(this)),
  _setRemoteDescriptionObserver(new rtc::RefCountedObject<SetRemoteDescriptionObserver>(this)),
  _rtcStatsCollectorCallback(new rtc::RefCountedObject<RTCStatsCollectorCallback>(this)),
  _dataChannelObserver(std::make_unique<DataChannelObserver>(this)),
  _peerConnectionObserver(std::make_shared<PeerConnectionObserver>(this)),
  _localSdp(nullptr),
  _remotePlayerId(remotePlayerId),
  _remotePlayerLogin(remotePlayerLogin),
  _createOffer(createOffer),
  _localUdpSocket(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_DGRAM)),
  _receivedOffer(false),
  _dataChannelIsOpen(false),
  _gameUdpAddress("127.0.0.1", gameUdpPort),
  _queue("Relay"),
  _isConnected(false),
  _iceState("none")
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
  if (_connection)
  {
    _connection->Close();
    _peerConnectionObserver.reset();
  }
}

void PeerRelay::reinit()
{
  _queue.PostDelayedTask(rtc::Bind(&PeerRelay::_checkConnectionTimeout, this), 1000);
  _connectStartTime = std::chrono::steady_clock::now();
  _isConnected = false;
  _initPeerConnection();
  if (_createOffer)
  {
    webrtc::DataChannelInit dataChannelInit;
    dataChannelInit.maxRetransmits = 0;
    dataChannelInit.ordered = false;
    _dataChannel = _connection->CreateDataChannel("faf",
                                                  &dataChannelInit);
    _dataChannel->RegisterObserver(_dataChannelObserver.get());
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    options.offer_to_receive_audio = 0;
    options.offer_to_receive_video = 0;
    _connection->CreateOffer(_createOfferObserver,
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
  result["ice_agent"]["datachannel_open"] = _dataChannelIsOpen;
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

void PeerRelay::setDataChannelOpenCallback(DataChannelOpenCallback cb)
{
  _dataChannelOpenCallback = cb;
}

void PeerRelay::setIceServers(webrtc::PeerConnectionInterface::IceServers const& iceServers)
{
  _iceServerList = iceServers;
}

void PeerRelay::addIceMessage(Json::Value const& iceMsg)
{
  if (!_connection)
  {
    return;
  }
  if (iceMsg["type"].asString() == "offer" ||
      iceMsg["type"].asString() == "answer")
  {
    webrtc::SdpParseError error;
    _connection->SetRemoteDescription(_setRemoteDescriptionObserver,
                                      webrtc::CreateSessionDescription(iceMsg["type"].asString(),
                                                                       iceMsg["sdp"].asString(),
                                                                       &error));
  }
  else if (iceMsg["type"].asString() == "candidate")
  {
    webrtc::SdpParseError error;
    _connection->AddIceCandidate(webrtc::CreateIceCandidate(iceMsg["candidate"]["sdpMid"].asString(),
                                                            iceMsg["candidate"]["sdpMLineIndex"].asInt(),
                                                            iceMsg["candidate"]["candidate"].asString(),
                                                            &error));
  }
}


void PeerRelay::_setIceState(std::string const& state)
{
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
  RELAY_LOG(LS_VERBOSE) << "ice state changed to" << state;
}

void PeerRelay::_setConnected(bool connected)
{
  if (connected && !_isConnected)
  {
    _connectDuration = std::chrono::steady_clock::now() - _connectStartTime;
    RELAY_LOG(LS_INFO) << "connected after " <<  std::chrono::duration_cast<std::chrono::milliseconds>(_connectDuration).count() / 1000.;
    _connection->GetStats(_rtcStatsCollectorCallback.get());
    if (_dataChannelIsOpen && _dataChannelOpenCallback)
    {
      _dataChannelOpenCallback();
    }
  }

  if (!connected)
  {
    RELAY_LOG(LS_INFO) << "disconnected";
  }

  _isConnected = connected;
}

void PeerRelay::_initPeerConnection()
{
  webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  configuration.servers = _iceServerList;
  _connection = _pcfactory->CreatePeerConnection(configuration,
                                                 nullptr,
                                                 nullptr,
                                                 _peerConnectionObserver.get());
/*
  webrtc::FakeConstraints constraints;
  constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, webrtc::MediaConstraintsInterface::kValueTrue);
  // FIXME: crashes without these constraints, why?
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveAudio, webrtc::MediaConstraintsInterface::kValueFalse);
  constraints.AddMandatory(webrtc::MediaConstraintsInterface::kOfferToReceiveVideo, webrtc::MediaConstraintsInterface::kValueFalse);


  _connection = _pcfactory->CreatePeerConnection(configuration,
                                                 &constraints,
                                                 nullptr,
                                                 nullptr,
                                                 _peerConnectionObserver.get());
*/
}

void PeerRelay::_checkConnectionTimeout()
{
  auto duration = std::chrono::steady_clock::now() - _connectStartTime;
  if (!_isConnected)
  {

  }
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
