
#include <iostream>
#include <unordered_map>
#include <memory>

#include <webrtc/rtc_base/scoped_ref_ptr.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/rtc_base/ssladapter.h>
#include <webrtc/rtc_base/thread.h>
#include <webrtc/media/engine/webrtcmediaengine.h>

#include "Timer.h"

class PeerConnection;
rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pcfactory;

static std::shared_ptr<PeerConnection> pc1, pc2;

class PeerConnectionObserver;
class DataChannelObserver;
class CreateOfferObserver;
class CreateAnswerObserver;
class SetLocalDescriptionObserver;
class SetRemoteDescriptionObserver;

class PeerConnection
{
public:
  std::unique_ptr<PeerConnectionObserver> pcObserver;
  std::unique_ptr<DataChannelObserver> dcObserver;
  rtc::scoped_refptr<CreateOfferObserver> offerObserver;
  rtc::scoped_refptr<CreateAnswerObserver> answerObserver;
  rtc::scoped_refptr<SetLocalDescriptionObserver> setLocalDescriptionObserver;
  rtc::scoped_refptr<SetRemoteDescriptionObserver> setRemoteDescriptionObserver;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peerConnection;
  rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel;

  PeerConnection();

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
    if (_pc == pc1.get())
    {
      pc2->addRemoteCandidate(candidate);
    }
    else
    {
      pc1->addRemoteCandidate(candidate);
    }
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
    pc1->peerConnection->SetLocalDescription(_pc->setLocalDescriptionObserver, sdp);
    pc2->createAnswer(sdp);
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
    pc1->peerConnection->SetRemoteDescription(pc1->setRemoteDescriptionObserver, sdp);
  }

  virtual void OnFailure(const std::string &msg) override
  {
    std::cout << "CreateAnswerObserver::OnFailure" << std::endl;
  }
};

PeerConnection::PeerConnection()
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
  dataChannel = peerConnection->CreateDataChannel("faf",
                                                  nullptr);

  dataChannel->RegisterObserver(dcObserver.get());
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
  if (pc1->dataChannel && pc1->dataChannel->state() == webrtc::DataChannelInterface::kOpen &&
      pc2->dataChannel && pc2->dataChannel->state() == webrtc::DataChannelInterface::kOpen)
  {
    std::cout << "all datachannels opened" << std::endl;
  }
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
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr,
                                                         nullptr);

  /* create all PeerConnections */
  pc1 = std::make_shared<PeerConnection>();
  pc2 = std::make_shared<PeerConnection>();
  pc1->createOffer();

  rtc::Thread::Current()->Run();
  return 0;
}
