#pragma once

#include <memory>
#include <functional>
#include <chrono>
#include <array>

#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/base/task_queue.h>

#include <third_party/json/json.h>

namespace faf {

class CreateOfferObserver;
class CreateAnswerObserver;
class SetLocalDescriptionObserver;
class SetRemoteDescriptionObserver;
class PeerConnectionObserver;
class DataChannelObserver;
class RTCStatsCollectorCallback;

class PeerRelay : public sigslot::has_slots<>
{
public:
  PeerRelay(int remotePlayerId,
            std::string const& remotePlayerLogin,
            bool createOffer,
            int gameUdpPort,
            rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> const& pcfactory);
  virtual ~PeerRelay();

  typedef std::function<void (Json::Value const& iceMsg)> IceMessageCallback;
  void setIceMessageCallback(IceMessageCallback cb);

  typedef std::function<void (std::string const& state)> StateCallback;
  void setStateCallback(StateCallback cb);

  typedef std::function<void ()> DataChannelOpenCallback;
  void setDataChannelOpenCallback(DataChannelOpenCallback cb);

  void setIceServers(webrtc::PeerConnectionInterface::IceServers const& iceServers);

  void addIceMessage(Json::Value const& iceMsg);

  void reinit();

  int localUdpSocketPort() const;

  Json::Value status() const;

protected:
  void _setIceState(std::string const& state);
  void _setConnected(bool connected);
  void _checkConnectionTimeout();
  void _onPeerdataFromGame(rtc::AsyncSocket* socket);

  /* runtime objects for WebRTC */
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _pcfactory;
  webrtc::PeerConnectionInterface::IceServers _iceServerList;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
  rtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;
  webrtc::SessionDescriptionInterface* _localSdp;

  /* Callback objects for WebRTC API calls */
  rtc::scoped_refptr<CreateOfferObserver> _createOfferObserver;
  rtc::scoped_refptr<CreateAnswerObserver> _createAnswerObserver;
  rtc::scoped_refptr<SetLocalDescriptionObserver> _setLocalDescriptionObserver;
  rtc::scoped_refptr<SetRemoteDescriptionObserver> _setRemoteDescriptionObserver;
  rtc::scoped_refptr<RTCStatsCollectorCallback> _rtcStatsCollectorCallback;
  std::unique_ptr<DataChannelObserver> _dataChannelObserver;
  std::shared_ptr<PeerConnectionObserver> _peerConnectionObserver;

  /* local identifying data */
  int _remotePlayerId;
  std::string _remotePlayerLogin;
  bool _createOffer;

  /* game P2P socket data */
  std::unique_ptr<rtc::AsyncSocket> _localUdpSocket;
  int _localUdpSocketPort;
  std::array<char, 2048> _readBuffer;
  rtc::SocketAddress _gameUdpAddress;

  /* callbacks */
  IceMessageCallback _iceMessageCallback;
  StateCallback _stateCallback;
  DataChannelOpenCallback _dataChannelOpenCallback;

  /* ICE state data */
  bool _receivedOffer;
  bool _dataChannelIsOpen;
  bool _isConnected;
  std::string _iceState;
  std::string _localCandAddress;
  std::string _remoteCandAddress;
  std::string _localCandType;
  std::string _remoteCandType;

  /* connectivity check data */
  rtc::TaskQueue _queue;
  std::chrono::steady_clock::time_point _connectStartTime;
  std::chrono::steady_clock::duration _connectDuration;
  std::chrono::steady_clock::duration _connectionAttemptTimeoutMs;

  /* access declarations for observers */
  friend CreateOfferObserver;
  friend CreateAnswerObserver;
  friend SetLocalDescriptionObserver;
  friend SetRemoteDescriptionObserver;
  friend PeerConnectionObserver;
  friend DataChannelObserver;
  friend RTCStatsCollectorCallback;
};

} // namespace faf
