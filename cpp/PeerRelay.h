#pragma once

#include <memory>
#include <functional>

#include <webrtc/api/peerconnectioninterface.h>
#include <third_party/json/json.h>

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

class PeerRelay : public sigslot::has_slots<>
{
public:
  PeerRelay(int remotePlayerId,
            std::string const& remotePlayerLogin,
            bool createOffer,
            int gameUdpPort);
  virtual ~PeerRelay();

  void setConnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection,
                     std::shared_ptr<PeerConnectionObserver> observer);

  int localUdpSocketPort() const;

  typedef std::function<void (Json::Value const& iceMsg)> IceMessageCallback;
  void setIceMessageCallback(IceMessageCallback cb);

  typedef std::function<void (std::string const& state)> StateCallback;
  void setStateCallback(StateCallback cb);

  typedef std::function<void ()> DataChannelOpenCallback;
  void setDataChannelOpenCallback(DataChannelOpenCallback cb);

  void addIceMessage(Json::Value const& iceMsg);

protected:
  void _onPeerdataFromGame(rtc::AsyncSocket* socket);

  rtc::scoped_refptr<CreateOfferObserver> _createOfferObserver;
  rtc::scoped_refptr<CreateAnswerObserver> _createAnswerObserver;
  rtc::scoped_refptr<SetLocalDescriptionObserver> _setLocalDescriptionObserver;
  rtc::scoped_refptr<SetRemoteDescriptionObserver> _setRemoteDescriptionObserver;
  std::unique_ptr<DataChannelObserver> _dataChannelObserver;
  std::shared_ptr<PeerConnectionObserver> _peerConnectionObserver;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _connection;
  rtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;
  std::unique_ptr<webrtc::SessionDescriptionInterface> _localSdp;
  int _remotePlayerId;
  std::string _remotePlayerLogin;
  bool _createOffer;
  std::unique_ptr<rtc::AsyncSocket> _localUdpSocket;
  int _localUdpSocketPort;
  std::array<char, 2048> _readBuffer;
  IceMessageCallback _iceMessageCallback;
  StateCallback _stateCallback;
  DataChannelOpenCallback _dataChannelOpenCallback;
  bool _receivedOffer;
  bool _dataChannelIsOpen;
  rtc::SocketAddress _gameUdpAddress;

  friend CreateOfferObserver;
  friend CreateAnswerObserver;
  friend SetLocalDescriptionObserver;
  friend SetRemoteDescriptionObserver;
  friend PeerConnectionObserver;
  friend DataChannelObserver;
};

} // namespace faf
