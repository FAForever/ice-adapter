#include "PeerRelay.h"

#include "IceAgent.h"
#include "logging.h"

namespace faf
{

PeerRelay::PeerRelay(Glib::RefPtr<Glib::MainLoop> mainloop,
                     int peerId,
                     std::string const& peerLogin,
                     std::string const& stunIp,
                     std::string const& turnIp,
                     SdpMessageCallback sdpCb,
                     IceAgentStateCallback stateCb,
                     CandidateSelectedCallback candSelCb,
                     bool createOffer,
                     IceAdapterOptions const& options):
  mMainloop(mainloop),
  mPeerId(peerId),
  mPeerLogin(peerLogin),
  mStunIp(stunIp),
  mTurnIp(turnIp),
  mLocalGameUdpPort(0),
  mCreateOffer(createOffer),
  mSdpMessageCallback(sdpCb),
  mIceAgentStateCallback(stateCb),
  mCandidateSelectedCallback(candSelCb),
  mOptions(options)
{
  createAgent();

  mLocalSocket = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                                Gio::SOCKET_TYPE_DATAGRAM,
                                Gio::SOCKET_PROTOCOL_DEFAULT);
  mLocalSocket->set_blocking(false);

  auto srcAddress = Gio::InetSocketAddress::create(
        Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4),
        static_cast<guint16>(mLocalGameUdpPort));
  mLocalSocket->bind(srcAddress, false);

  auto isockaddr = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(mLocalSocket->get_local_address());
  if (isockaddr)
  {
    mLocalGameUdpPort = isockaddr->get_port();
    FAF_LOG_TRACE << "PeerRelay for player " << peerId << " listening on port " << mLocalGameUdpPort;
  }
  else
  {
    FAF_LOG_ERROR << "!isockaddr";
  }

  Gio::signal_socket().connect(
        sigc::mem_fun(this, &PeerRelay::onGameReceive),
        mLocalSocket,
        Glib::IO_IN);

  mGameAddress = Gio::InetSocketAddress::create(Gio::InetAddress::create("127.0.0.1"),
                                                static_cast<guint16>(mOptions.gameUdpPort));
  FAF_LOG_TRACE << "PeerRelay " << mPeerId << " constructed";
}

PeerRelay::~PeerRelay()
{
  FAF_LOG_TRACE << "PeerRelay " << mPeerId << " destructed";
}

void PeerRelay::setPeer(int peerId,
                        std::string const& peerLogin)
{
  mPeerId = peerId;
  mPeerLogin = peerLogin;
}

int PeerRelay::peerId() const
{
  return mPeerId;
}

int PeerRelay::localGameUdpPort() const
{
  return mLocalGameUdpPort;
}

std::shared_ptr<IceAgent> PeerRelay::iceAgent() const
{
  return mIceAgent;
}

std::string const& PeerRelay::peerLogin() const
{
  return mPeerLogin;
}

void PeerRelay::reconnect()
{
  /*
  std::string oldSdp;
  if (mIceAgent &&
      mIceAgent->hasRemoteSdp())
  {
    oldSdp = mIceAgent->remoteSdp64();
  }
  */
  createAgent();
  /*
  if (!oldSdp.empty())
  {
    mIceAgent->setRemoteSdp(oldSdp);
  }
  */
}

bool PeerRelay::isOfferer() const
{
  return mCreateOffer;
}

void PeerRelay::createAgent()
{
  mIceAgent.reset();
  mIceAgent = std::make_shared<IceAgent>(mMainloop->gobj(),
                                         mCreateOffer,
                                         mPeerId,
                                         mStunIp,
                                         mTurnIp,
                                         mOptions);

  mIceAgent->setReceiveCallback([this](IceAgent* agent, std::string const& message)
  {
    FAF_LOG_TRACE << "relaying " << message.size() << " bytes from peer to game";
    mLocalSocket->send_to(mGameAddress,
                          message.c_str(),
                          message.size());
  });

  mIceAgent->setStateCallback([this](IceAgent* agent, IceAgentState const& state)
  {
    mIceAgentStateCallback(this, state);
  });

  mIceAgent->setSdpMessageCallback([this](IceAgent*, std::string const& type, std::string const& msg)
  {
    mSdpMessageCallback(this, type, msg);
  });

  mIceAgent->setCandidateSelectedCallback([this](IceAgent*, std::string const& local, std::string const& remote)
  {
    mCandidateSelectedCallback(this, local, remote);
  });
}

Glib::ustring
socket_address_to_string(const Glib::RefPtr<Gio::SocketAddress>& address)
{
  auto isockaddr = Glib::RefPtr<Gio::InetSocketAddress>::cast_dynamic(address);
  if (!isockaddr)
    return Glib::ustring();

  auto inet_address = isockaddr->get_address();
  auto str = inet_address->to_string();
  auto the_port = isockaddr->get_port();
  auto res = Glib::ustring::compose("%1:%2", str, the_port);
  return res;
}

bool PeerRelay::onGameReceive(Glib::IOCondition condition)
{
  gchar buffer[4096] = {};
  Glib::RefPtr<Gio::SocketAddress> address;
  //auto address = mLocalSocket->get_remote_address();
  auto size = mLocalSocket->receive_from(address,
                                         buffer,
                                         4096);
  //FAF_LOG_TRACE << "relaying " << size << " bytes from game to peer";
  if (mIceAgent &&
      mIceAgent->isConnected())
  {
    mIceAgent->send(std::string(buffer,
                                size));
  }
  else
  {
    FAF_LOG_DEBUG << "dropping " << size << " game bytes while waiting for ICE connection";
  }
  return true;
}

}
