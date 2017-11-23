#include "PeerRelayObservers.h"

#include <webrtc/api/stats/rtcstats_objects.h>

#include "logging.h"
#include "PeerRelay.h"

namespace faf {

#define OBSERVER_LOG(x) LOG(x) << "PeerRelay for " << _relay->_remotePlayerLogin << " (" << _relay->_remotePlayerId << "): "

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface *sdp)
{
  //FAF_LOG_DEBUG << "CreateOfferObserver::OnSuccess";
  if (_relay->_peerConnection)
  {
    _relay->_localSdp = sdp;
    _relay->_peerConnection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
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
  if (_relay->_peerConnection)
  {
    _relay->_localSdp = sdp;
    _relay->_peerConnection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
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
  if (_relay->_peerConnection &&
      !_relay->_createOffer)
  {
    _relay->_peerConnection->CreateAnswer(_relay->_createAnswerObserver,
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
        _relay->_setConnected(true);
        break;
      case webrtc::DataChannelInterface::kClosing:
        //FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closing";
        break;
      case webrtc::DataChannelInterface::kClosed:
        //FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closed";
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

} // namespace faf
