#include "PeerRelay.h"

#include <webrtc/api/mediaconstraintsinterface.h>
#include <webrtc/api/test/fakeconstraints.h>

#include "logging.h"

namespace faf {

void CreateOfferObserver::OnSuccess(webrtc::SessionDescriptionInterface *sdp)
{
  FAF_LOG_DEBUG << "CreateOfferObserver::OnSuccess";
  if (_relay->_connection)
  {
    _relay->_localSdp = std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp);
    _relay->_connection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
                                             sdp);
  }
}

void CreateOfferObserver::OnFailure(const std::string &msg)
{
  FAF_LOG_DEBUG << "CreateOfferObserver::OnFailure: " << msg;
}

void CreateAnswerObserver::OnSuccess(webrtc::SessionDescriptionInterface *sdp)
{
  FAF_LOG_DEBUG << "CreateAnswerObserver::OnSuccess";
  if (_relay->_connection)
  {
    _relay->_localSdp = std::unique_ptr<webrtc::SessionDescriptionInterface>(sdp);
    _relay->_connection->SetLocalDescription(_relay->_setLocalDescriptionObserver,
                                             sdp);
  }
}

void CreateAnswerObserver::OnFailure(const std::string &msg)
{
  FAF_LOG_DEBUG << "CreateAnswerObserver::OnFailure: " << msg;
}

void SetLocalDescriptionObserver::OnSuccess()
{
  FAF_LOG_DEBUG << "SetLocalDescriptionObserver::OnSuccess";
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
  FAF_LOG_DEBUG << "SetLocalDescriptionObserver::OnFailure";
}

void SetRemoteDescriptionObserver::OnSuccess()
{
  FAF_LOG_DEBUG << "SetRemoteDescriptionObserver::OnSuccess";
  if (_relay->_connection &&
      !_relay->_createOffer)
  {
    _relay->_connection->CreateAnswer(_relay->_createAnswerObserver,
                                      nullptr);
  }
}

void SetRemoteDescriptionObserver::OnFailure(const std::string &msg)
{
  FAF_LOG_DEBUG << "SetRemoteDescriptionObserver::OnFailure";
}

void PeerConnectionObserver::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state)
{
  FAF_LOG_DEBUG << "PeerConnectionObserver::OnSignalingChange";
}

void PeerConnectionObserver::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
  if (_relay->_stateCallback)
  {
    switch (new_state)
    {
      case webrtc::PeerConnectionInterface::kIceConnectionNew:
        _relay->_stateCallback("new");
        FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange changed to new";
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      _relay->_stateCallback("checking");
      FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange changed to checking";
      break;
      case webrtc::PeerConnectionInterface::kIceConnectionConnected:
        _relay->_stateCallback("connected");
        FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange changed to connected";
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
        _relay->_stateCallback("completed");
        FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange changed to completed";
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionFailed:
        _relay->_stateCallback("failed");
        FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange changed to failed";
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
        _relay->_stateCallback("disconnected");
        FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange changed to disconnected";
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionClosed:
        _relay->_stateCallback("closed");
        FAF_LOG_DEBUG << "PeerConnectionObserver::OnIceConnectionChange changed to closed";
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionMax:
        /* not in https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState */
        break;
    }
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
        FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Connecting";
        break;
      case webrtc::DataChannelInterface::kOpen:
        FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Open";
        _relay->_dataChannelIsOpen = true;
        if (_relay->_dataChannelOpenCallback)
        {
          _relay->_dataChannelOpenCallback();
        }
        break;
      case webrtc::DataChannelInterface::kClosing:
        FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closing";
        _relay->_dataChannelIsOpen = false;
        break;
      case webrtc::DataChannelInterface::kClosed:
        FAF_LOG_DEBUG << "DataChannelObserver::OnStateChange to Closed";
        _relay->_dataChannelIsOpen = false;
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

PeerRelay::PeerRelay(int remotePlayerId,
                     std::string const& remotePlayerLogin,
                     bool createOffer,
                     int gameUdpPort):
  _createOfferObserver(new rtc::RefCountedObject<CreateOfferObserver>(this)),
  _createAnswerObserver(new rtc::RefCountedObject<CreateAnswerObserver>(this)),
  _setLocalDescriptionObserver(new rtc::RefCountedObject<SetLocalDescriptionObserver>(this)),
  _setRemoteDescriptionObserver(new rtc::RefCountedObject<SetRemoteDescriptionObserver>(this)),
  _dataChannelObserver(std::make_unique<DataChannelObserver>(this)),
  _remotePlayerId(remotePlayerId),
  _remotePlayerLogin(remotePlayerLogin),
  _createOffer(createOffer),
  _localUdpSocket(rtc::Thread::Current()->socketserver()->CreateAsyncSocket(SOCK_DGRAM)),
  _receivedOffer(false),
  _dataChannelIsOpen(false),
  _gameUdpAddress("localhost", gameUdpPort)
{
  _localUdpSocket->SignalReadEvent.connect(this, &PeerRelay::_onPeerdataFromGame);
  if (_localUdpSocket->Bind(rtc::SocketAddress("127.0.0.1", 0)) != 0)
  {
    FAF_LOG_ERROR << "unable to bind local udp socket";
  }
  _localUdpSocketPort = _localUdpSocket->GetLocalAddress().port();
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
  }
}

void PeerRelay::setConnection(rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection,
                              std::shared_ptr<PeerConnectionObserver> observer)
{
  _connection = connection;
  _peerConnectionObserver = observer;

  if (_createOffer)
  {
    webrtc::DataChannelInit dataChannelInit;
    dataChannelInit.maxRetransmits = 0;
    dataChannelInit.ordered = false;
    _dataChannel = _connection->CreateDataChannel("faf",
                                                  &dataChannelInit);
    _dataChannel->RegisterObserver(_dataChannelObserver.get());
    _connection->CreateOffer(_createOfferObserver,
                             nullptr);
  }
}

int PeerRelay::localUdpSocketPort() const
{
  return _localUdpSocketPort;
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

void PeerRelay::_onPeerdataFromGame(rtc::AsyncSocket* socket)
{
  std::size_t msgLength = socket->Recv(_readBuffer.data(), _readBuffer.size(), nullptr);
  if (!_dataChannel)
  {
    FAF_LOG_TRACE << "skipping " << msgLength << " bytes of P2P data until ICE connection is established";
    return;
  }
  _dataChannel->Send(webrtc::DataBuffer(rtc::CopyOnWriteBuffer(_readBuffer.data(), msgLength), true));
}

} // namespace faf
