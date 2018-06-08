
#include <iostream>
#include <unordered_map>
#include <memory>

#include <webrtc/rtc_base/scoped_ref_ptr.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/rtc_base/ssladapter.h>
#include <webrtc/rtc_base/thread.h>
#include <webrtc/media/engine/webrtcmediaengine.h>

class PeerConnection;
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcfactory;

typedef std::size_t PeerId;

static constexpr std::size_t numPeers = 2;
std::set<std::pair<PeerId, PeerId>> offers;

struct PeerIdPairHash {
public:
  std::size_t operator()(const std::pair<PeerId, PeerId> &x) const
  {
    return std::hash<PeerId>()(x.first) ^ std::hash<PeerId>()(x.second);
  }
};

std::unordered_map<std::pair<PeerId, PeerId>, std::shared_ptr<PeerConnection>, PeerIdPairHash> directedPeerConnections;

class PeerConnectionObserver;
class DataChannelObserver;
class CreateOfferObserver;
class CreateAnswerObserver;
class SetLocalDescriptionObserver;
class SetRemoteDescriptionObserver;

class PeerConnection
{
public:
  PeerId localId;
  PeerId remoteId;

  std::unique_ptr<PeerConnectionObserver> pcObserver;
  std::unique_ptr<DataChannelObserver> dcObserver;
  rtc::scoped_refptr<CreateOfferObserver> offerObserver;
  rtc::scoped_refptr<CreateAnswerObserver> answerObserver;
  rtc::scoped_refptr<SetLocalDescriptionObserver> setLocalDescriptionObserver;
  rtc::scoped_refptr<SetRemoteDescriptionObserver> setRemoteDescriptionObserver;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel;

  PeerConnection(PeerId localId,
                 PeerId remoteId);

  ~PeerConnection();

  void createOffer();

  void createAnswer(webrtc::SessionDescriptionInterface *offerSdp);

  void addRemoteCandidate(const webrtc::IceCandidateInterface *candidate);
};

void checkDatachannels();

class DataChannelObserver : public webrtc::DataChannelObserver
{
private:
  PeerConnection* _pc;

 public:
  explicit DataChannelObserver(PeerConnection*  pc) : _pc(pc) {}
  virtual ~DataChannelObserver() override {}

  virtual void OnStateChange() override
  {
    std::cout << "DataChannelObserver::OnStateChange" << std::endl;
    if (_pc->dataChannel->state() == webrtc::DataChannelInterface::kOpen)
    {
      //std::cout << "datachannel " << _pc->localId << " -> " << _pc->remoteId << " opened" << std::endl;
      checkDatachannels();
    }
  }

  virtual void OnMessage(const webrtc::DataBuffer& buffer) override
  {
    std::cout << "DataChannelObserver::OnMessage" << std::endl;
  }
};

class PeerConnectionObserver : public webrtc::PeerConnectionObserver
{
private:
 PeerConnection* _pc;

public:
  explicit PeerConnectionObserver(PeerConnection*  pc) : _pc(pc) {}
  virtual ~PeerConnectionObserver(){};

  virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override
  {
    std::cout << "PeerConnectionObserver::OnSignalingChange" << std::endl;
  }
  virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override
  {
    std::cout << "PeerConnectionObserver::OnIceConnectionChange " << static_cast<int>(new_state) << std::endl;
    switch (new_state)
    {
      case webrtc::PeerConnectionInterface::kIceConnectionNew:
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionChecking:
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionConnected:
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionFailed:
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionClosed:
        break;
      case webrtc::PeerConnectionInterface::kIceConnectionMax:
        /* not in https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/iceConnectionState */
        break;
    }
  }
  virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override
  {
    std::cout << "PeerConnectionObserver::OnIceGatheringChange " << static_cast<int>(new_state) << std::endl;
  }
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override
  {
    std::cout << "PeerConnectionObserver::OnIceCandidate" << std::endl;
    directedPeerConnections.at({_pc->remoteId, _pc->localId})->addRemoteCandidate(candidate);
  }
  virtual void OnRenegotiationNeeded() override
  {
    std::cout << "PeerConnectionObserver::OnRenegotiationNeeded" << std::endl;
  }
  virtual void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override
  {
    std::cout << "PeerConnectionObserver::OnDataChannel" << std::endl;
    _pc->dataChannel = data_channel;
    data_channel->RegisterObserver( _pc->dcObserver.get());
  }
  virtual void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override
  {
    std::cout << "PeerConnectionObserver::OnAddStream" << std::endl;
  }
  virtual void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override
  {
    std::cout << "PeerConnectionObserver::OnRemoveStream" << std::endl;
  }
};

class SetLocalDescriptionObserver : public webrtc::SetSessionDescriptionObserver
{
private:
  PeerConnection* _pc;

public:
  explicit SetLocalDescriptionObserver(PeerConnection*  pc) : _pc(pc) {}
  virtual ~SetLocalDescriptionObserver() override {}

  virtual void OnSuccess() override
  {
    std::cout << "SetLocalDescriptionObserver::OnSuccess" << std::endl;
  }

  virtual void OnFailure(const std::string &msg) override
  {
    std::cout << "SetLocalDescriptionObserver::OnFailure" << std::endl;
  }
};

class SetRemoteDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
private:
  PeerConnection* _pc;

public:
  explicit SetRemoteDescriptionObserver(PeerConnection*  pc) : _pc(pc) {}
  virtual ~SetRemoteDescriptionObserver() override {}

  virtual void OnSuccess() override
  {
    std::cout << "SetRemoteDescriptionObserver::OnSuccess" << std::endl;
  }

  virtual void OnFailure(const std::string &msg) override
  {
    std::cout << "SetRemoteDescriptionObserver::OnFailure" << std::endl;
  }

};

class CreateOfferObserver : public webrtc::CreateSessionDescriptionObserver
{
private:
  PeerConnection* _pc;

public:
  explicit CreateOfferObserver(PeerConnection*  pc) : _pc(pc) {}
  virtual ~CreateOfferObserver() override {}

  virtual void OnSuccess(webrtc::SessionDescriptionInterface *sdp) override
  {
    std::cout << "CreateOfferObserver::OnSuccess" << std::endl;
    _pc->peerConnection->SetLocalDescription(_pc->setLocalDescriptionObserver, sdp);
    directedPeerConnections.at({_pc->remoteId, _pc->localId})->createAnswer(sdp);
  }

  virtual void OnFailure(const std::string &msg) override
  {
    std::cout << "CreateOfferObserver::OnFailure" << std::endl;
  }
};

class CreateAnswerObserver : public webrtc::CreateSessionDescriptionObserver
{
private:
  PeerConnection* _pc;

public:
  explicit CreateAnswerObserver(PeerConnection*  pc) : _pc(pc) {}
  virtual ~CreateAnswerObserver() override {}

  virtual void OnSuccess(webrtc::SessionDescriptionInterface *sdp) override
  {
    std::cout << "CreateAnswerObserver::OnSuccess" << std::endl;
    _pc->peerConnection->SetLocalDescription(_pc->setLocalDescriptionObserver, sdp);
    auto remotePc = directedPeerConnections.at({_pc->remoteId, _pc->localId});
    remotePc->peerConnection->SetRemoteDescription(remotePc->setRemoteDescriptionObserver, sdp);
  }

  virtual void OnFailure(const std::string &msg) override
  {
    std::cout << "CreateAnswerObserver::OnFailure" << std::endl;
  }
};

PeerConnection::PeerConnection(PeerId localId,
                 PeerId remoteId):
    localId(localId),
    remoteId(remoteId)
{
  pcObserver = std::make_unique<PeerConnectionObserver>(this);
  dcObserver = std::make_unique<DataChannelObserver>(this);
  offerObserver = new rtc::RefCountedObject<CreateOfferObserver>(this);
  answerObserver = new rtc::RefCountedObject<CreateAnswerObserver>(this);
  setLocalDescriptionObserver = new rtc::RefCountedObject<SetLocalDescriptionObserver>(this);
  setRemoteDescriptionObserver = new rtc::RefCountedObject<SetRemoteDescriptionObserver>(this);

  webrtc::PeerConnectionInterface::RTCConfiguration configuration;
  peerConnection = pcfactory->CreatePeerConnection(configuration,
                                                   nullptr,
                                                   nullptr,
                                                   pcObserver.get());
}

PeerConnection::~PeerConnection()
{
  if (dataChannel)
  {
    dataChannel->UnregisterObserver();
    dataChannel->Close();
    dataChannel.release();
  }
  if (peerConnection)
  {
    peerConnection->Close();
    peerConnection.release();
  }
}

void PeerConnection::createOffer()
{
  //webrtc::DataChannelInit dataChannelInit;
  //dataChannelInit.maxRetransmits = 0;
  //dataChannelInit.ordered = false;
  dataChannel = peerConnection->CreateDataChannel("faf",
                                                  nullptr);

  dataChannel->RegisterObserver(dcObserver.get());
  //webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  //options.offer_to_receive_audio = 0;
  //options.offer_to_receive_video = 0;
  peerConnection->CreateOffer(offerObserver.get(),
                              nullptr);
}

void PeerConnection::createAnswer(webrtc::SessionDescriptionInterface *offerSdp)
{
  peerConnection->SetRemoteDescription(setRemoteDescriptionObserver, offerSdp);
  peerConnection->CreateAnswer(answerObserver, nullptr);
}

void PeerConnection::addRemoteCandidate(const webrtc::IceCandidateInterface *candidate)
{
  std::cout << "PeerConnection::addRemoteCandidate" << std::endl;
  if (!peerConnection->local_description())
  {
    std::cerr << "missing local sdp" << std::endl;
  }
  if (!peerConnection->remote_description())
  {
    std::cerr << "missing remote sdp" << std::endl;
  }
  peerConnection->AddIceCandidate(candidate);
}

void checkDatachannels()
{
  for (PeerId localId = 0; localId < numPeers; ++localId)
  {
    for (PeerId remoteId = 0; remoteId < numPeers; ++remoteId)
    {
      if (localId != remoteId)
      {
        auto pc = directedPeerConnections.at({localId, remoteId});
        if (pc->dataChannel && pc->dataChannel->state() != webrtc::DataChannelInterface::kOpen)
        {
          return;
        }
      }
    }
  }
  std::cout << "all " << directedPeerConnections.size() << " datachannels opened" << std::endl;
}

int main(int argc, char *argv[])
{
  if (!rtc::InitializeSSL())
  {
    std::cerr << "Error in InitializeSSL()";
    std::exit(1);
  }

  pcfactory = webrtc::CreateModularPeerConnectionFactory(nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr);

  /* create all PeerConnections */
  for (PeerId localId = 0; localId < numPeers; ++localId)
  {
    for (PeerId remoteId = 0; remoteId < numPeers; ++remoteId)
    {
      if (localId != remoteId)
      {
        auto directionPair = std::make_pair(localId, remoteId);
        auto pc = std::make_shared<PeerConnection>(localId, remoteId);
        if (offers.find(directionPair) == offers.end())
        {
          pc->createOffer();
          offers.insert(directionPair);
          offers.insert({remoteId, localId});
        }
        directedPeerConnections.insert({{localId, remoteId}, pc});
      }
    }
  }

  rtc::Thread::Current()->Run();
  return 0;
}
