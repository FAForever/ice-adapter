#pragma once

#include <webrtc/api/peerconnectioninterface.h>

namespace faf {

class PeerRelay;

class CreateOfferObserver : public webrtc::CreateSessionDescriptionObserver
{
private:
  PeerRelay* _relay;

public:
  explicit CreateOfferObserver(PeerRelay *relay) : _relay(relay) {}

  virtual void OnSuccess(webrtc::SessionDescriptionInterface *sdp) override;
  virtual void OnFailure(const std::string &msg) override;
};

class CreateAnswerObserver : public webrtc::CreateSessionDescriptionObserver
{
private:
  PeerRelay* _relay;

public:
  explicit CreateAnswerObserver(PeerRelay *relay) : _relay(relay) {}

  virtual void OnSuccess(webrtc::SessionDescriptionInterface *sdp) override;
  virtual void OnFailure(const std::string &msg) override;
};

class SetLocalDescriptionObserver : public webrtc::SetSessionDescriptionObserver
{
private:
  PeerRelay* _relay;

public:
  explicit SetLocalDescriptionObserver(PeerRelay *relay)
      : _relay(relay) {}

  virtual void OnSuccess() override;
  virtual void OnFailure(const std::string &msg) override;
};

class SetRemoteDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
private:
  PeerRelay* _relay;

public:
  explicit SetRemoteDescriptionObserver(PeerRelay *relay)
      : _relay(relay) {}

  virtual void OnSuccess() override;
  virtual void OnFailure(const std::string &msg) override;
};

class PeerConnectionObserver : public webrtc::PeerConnectionObserver
{
private:
  PeerRelay* _relay;

public:
  explicit PeerConnectionObserver(PeerRelay *relay) : _relay(relay) {}

  virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
  virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
  virtual void OnRenegotiationNeeded() override;
  virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
};

class DataChannelObserver : public webrtc::DataChannelObserver
{
private:
  PeerRelay* _relay;

 public:
  explicit DataChannelObserver(PeerRelay *relay) : _relay(relay) {}

  virtual void OnStateChange() override;
  virtual void OnMessage(const webrtc::DataBuffer& buffer) override;
};

class RTCStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback
{
private:
  PeerRelay* _relay;

 public:
  explicit RTCStatsCollectorCallback(PeerRelay *relay) : _relay(relay) {}

  virtual void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
};

} // namespace faf
