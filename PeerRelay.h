#pragma once

#include <memory>
#include <functional>
#include <chrono>
#include <array>

#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/rtc_base/copyonwritebuffer.h>

#include <webrtc/third_party/jsoncpp/source/include/json/json.h>

#include "Timer.h"
#include "PeerConnectivityChecker.h"

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
  struct Callbacks
  {
    std::function<void (Json::Value iceMsg)> iceMessageCallback;
    std::function<void (std::string state)> stateCallback;
    std::function<void (bool)> connectedCallback;
  };

  struct Options
  {
    int remotePlayerId;
    std::string remotePlayerLogin;
    bool isOfferer;
    int gameUdpPort;
    webrtc::PeerConnectionInterface::IceServers iceServers;
  };

  PeerRelay(Options options,
            Callbacks callbacks,
            rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> const& pcfactory);
  virtual ~PeerRelay();

  void setIceServers(webrtc::PeerConnectionInterface::IceServers const& iceServers);

  void addIceMessage(Json::Value const& iceMsg);

  int localUdpSocketPort() const;

  Json::Value status() const;

  bool isConnected() const;

protected:
  void _createOffer();
  void _setIceState(std::string const& state);
  void _setConnected(bool connected);
  void _onPeerdataFromGame(rtc::AsyncSocket* socket);
  void _onRemoteMessage(const uint8_t* data, std::size_t size);

  /* runtime objects for WebRTC */
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _pcfactory;
  webrtc::PeerConnectionInterface::IceServers _iceServerList;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
  rtc::scoped_refptr<webrtc::DataChannelInterface> _dataChannel;

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
  bool _isOfferer;

  /* game P2P socket data */
  rtc::SocketAddress _gameUdpAddress;
  std::unique_ptr<rtc::AsyncSocket> _localUdpSocket;
  int _localUdpSocketPort;
  static constexpr const std::size_t sendBufferSize = 2048;
  rtc::CopyOnWriteBuffer _sendCowBuffer{sendBufferSize};

  /* ICE state data */
  Callbacks _callbacks;
  bool _isConnected{false};
  bool _closing{false};
  std::string _iceState{"none"};
  std::string _localCandAddress;
  std::string _remoteCandAddress;
  std::string _localCandType;
  std::string _remoteCandType;
  std::string _localSdp;
  std::string _iceGatheringState{"none"};
  std::string _dataChannelState{"none"};
  std::chrono::steady_clock::time_point _connectStartTime;
  std::chrono::steady_clock::duration _connectDuration;
  std::unique_ptr<PeerConnectivityChecker> _connectionChecker;
  Timer _createNewOfferTimer;

  /* access declarations for observers */
  friend CreateOfferObserver;
  friend CreateAnswerObserver;
  friend SetLocalDescriptionObserver;
  friend SetRemoteDescriptionObserver;
  friend PeerConnectionObserver;
  friend DataChannelObserver;
  friend RTCStatsCollectorCallback;

  RTC_DISALLOW_COPY_AND_ASSIGN(PeerRelay);
};

} // namespace faf
