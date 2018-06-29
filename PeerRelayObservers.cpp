#include "PeerRelayObservers.h"

#include <webrtc/api/stats/rtcstats_objects.h>

#include "logging.h"
#include "PeerRelay.h"

namespace faf {


#define OBSERVER_LOG_ERROR FAF_LOG_ERROR << "PeerRelay for " << _relay->_remotePlayerLogin << " (" << _relay->_remotePlayerId << "): "
#define OBSERVER_LOG_WARN FAF_LOG_WARN << "PeerRelay for " << _relay->_remotePlayerLogin << " (" << _relay->_remotePlayerId << "): "
#define OBSERVER_LOG_INFO FAF_LOG_INFO << "PeerRelay for " << _relay->_remotePlayerLogin << " (" << _relay->_remotePlayerId << "): "
#define OBSERVER_LOG_DEBUG FAF_LOG_DEBUG << "PeerRelay for " << _relay->_remotePlayerLogin << " (" << _relay->_remotePlayerId << "): "
#define OBSERVER_LOG_TRACE FAF_LOG_TRACE << "PeerRelay for " << _relay->_remotePlayerLogin << " (" << _relay->_remotePlayerId << "): "

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface *sdp)
{
  OBSERVER_LOG_TRACE << "CreateOfferObserver::OnSuccess";
  sdp->ToString(&_relay->_localSdp);
  _relay->_peerConnection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
                                               sdp);
}

void CreateOfferObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG_WARN << "CreateOfferObserver::OnFailure: " << msg;
}

void CreateAnswerObserver::OnSuccess(webrtc::SessionDescriptionInterface *sdp)
{
  OBSERVER_LOG_TRACE << "CreateAnswerObserver::OnSuccess";
  sdp->ToString(&_relay->_localSdp);
  _relay->_peerConnection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
                                               sdp);
}

void CreateAnswerObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG_WARN << "CreateAnswerObserver::OnFailure: " << msg;
}

void SetLocalDescriptionObserver::OnSuccess()
{
  OBSERVER_LOG_DEBUG << "SetLocalDescriptionObserver::OnSuccess";
  if (_relay->_callbacks.iceMessageCallback)
  {
    Json::Value iceMsg;
    std::string sdpString;
    iceMsg["type"] = _relay->_isOfferer ? "offer" : "answer";
    iceMsg["sdp"] = _relay->_localSdp;
    _relay->_callbacks.iceMessageCallback(iceMsg);
  }
}

void SetLocalDescriptionObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG_WARN << "SetLocalDescriptionObserver::OnFailure: " << msg;
}

void SetRemoteDescriptionObserver::OnSuccess()
{
  OBSERVER_LOG_DEBUG << "SetRemoteDescriptionObserver::OnSuccess";
  if (!_relay->_isOfferer)
  {
    _relay->_peerConnection->CreateAnswer(_relay->_createAnswerObserver,
                                          nullptr);
  }
}

void SetRemoteDescriptionObserver::OnFailure(const std::string &msg)
{
  OBSERVER_LOG_WARN << "SetRemoteDescriptionObserver::OnFailure: " << msg;
}

void PeerConnectionObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnSignalingChange";
}

void PeerConnectionObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange" << static_cast<int>(new_state);
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
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnIceGatheringChange" << static_cast<int>(new_state);
  switch(new_state)
  {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      _relay->_iceGatheringState = "new";
      break;
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      _relay->_iceGatheringState = "gathering";
      break;
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      _relay->_iceGatheringState = "complete";
      break;
  }
}

void PeerConnectionObserver::OnIceCandidate(const webrtc::IceCandidateInterface *candidate)
{
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnIceCandidate";

  if (_relay->_callbacks.iceMessageCallback)
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
    _relay->_callbacks.iceMessageCallback(iceMsg);
  }
}

void PeerConnectionObserver::OnRenegotiationNeeded()
{
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnRenegotiationNeeded";
}

void PeerConnectionObserver::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
{
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnDataChannel";
  _relay->_dataChannel = data_channel;
  _relay->_dataChannel->RegisterObserver(_relay->_dataChannelObserver.get());
}

void PeerConnectionObserver::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnAddStream";
}

void PeerConnectionObserver::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{
  OBSERVER_LOG_DEBUG << "PeerConnectionObserver::OnRemoveStream";
}

void DataChannelObserver::OnStateChange()
{
  if (_relay->_dataChannel)
  {
    switch(_relay->_dataChannel->state())
    {
      case webrtc::DataChannelInterface::kOpen:
        OBSERVER_LOG_DEBUG << "DataChannelObserver::OnStateChange to Open";
        _relay->_dataChannelState = "open";
        break;
      case webrtc::DataChannelInterface::kConnecting:
        OBSERVER_LOG_DEBUG << "DataChannelObserver::OnStateChange to Connecting";
        _relay->_dataChannelState = "connecting";
        break;
      case webrtc::DataChannelInterface::kClosing:
        OBSERVER_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closing";
        _relay->_dataChannelState = "closing";
        break;
      case webrtc::DataChannelInterface::kClosed:
        OBSERVER_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closed";
        _relay->_dataChannelState = "closed";
        break;
    }
  }
}

void DataChannelObserver::OnMessage(const webrtc::DataBuffer& buffer)
{
  _relay->_onRemoteMessage(buffer.data.cdata(),
                           buffer.data.size());
}

void RTCStatsCollectorCallback::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report)
{
  OBSERVER_LOG_DEBUG << "RTCStatsCollectorCallback::OnStatsDelivered";
  if (!report)
  {
    OBSERVER_LOG_ERROR << "!report";
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
    //OBSERVER_LOG_DEBUG << "*pair->state ==" << *pair->state << "nominated ==" << *pair->nominated;
  }
  if (localCandId.empty())
  {
    OBSERVER_LOG_WARN << "localCandId.empty()";
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

} // namespace faf
