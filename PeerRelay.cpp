#include "PeerRelay.h"

#include "IceAgent.h"
#include "logging.h"

PeerRelay::PeerRelay(Glib::RefPtr<Glib::MainLoop> mainloop,
                     int gamePort,
                     int peerId,
                     std::string const& peerLogin,
                     std::string const& stunIp,
                     std::string const& turnIp,
                     std::string const& turnUser,
                     std::string const& turnPassword):
  mMainloop(mainloop),
  mPeerId(peerId),
  mPeerLogin(peerLogin),
  mLocalGameUdpPort(0)
{
  mIceAgent = std::make_shared<IceAgent>(mainloop->gobj(),
                                         true,
                                         stunIp,
                                         turnIp,
                                         turnUser,
                                         turnPassword);

  mIceAgent->setReceiveCallback([this](IceAgent* agent, std::string const& message)
  {
    FAF_LOG_TRACE << "relaying " << message.size() << " bytes from peer to game";
    mLocalSocket->send_to(mGameAddress,
                          message.c_str(),
                          message.size());
  });

  mIceAgent->setStateCallback([this](IceAgent* agent, IceAgentState const& state)
  {
    if (mIceAgentStateCallback)
    {
      mIceAgentStateCallback(this, state);
    }
  });

  mLocalSocket = Gio::Socket::create(Gio::SOCKET_FAMILY_IPV4,
                                Gio::SOCKET_TYPE_DATAGRAM,
                                Gio::SOCKET_PROTOCOL_DEFAULT);
  mLocalSocket->set_blocking(false);

  auto srcAddress = Gio::InetSocketAddress::create(
        Gio::InetAddress::create_loopback(Gio::SOCKET_FAMILY_IPV4),
        mLocalGameUdpPort);
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
                                                gamePort);
  FAF_LOG_TRACE << "PeerRelay " << mPeerId << " constructed";
}

PeerRelay::~PeerRelay()
{
  FAF_LOG_TRACE << "PeerRelay " << mPeerId << " destructed";
}

void PeerRelay::gatherCandidates(CandidateGatheringDoneCallback cb)
{
  mIceAgent->setCandidateGatheringDoneCallback([this, cb](IceAgent* agent, std::string const& sdp)
  {
    cb(this, sdp);
  });
  mIceAgent->gatherCandidates();
}

void PeerRelay::setIceAgentStateCallback(IceAgentStateCallback cb)
{
  mIceAgentStateCallback = cb;
}

int PeerRelay::localGameUdpPort() const
{
  return mLocalGameUdpPort;
}

std::shared_ptr<IceAgent> PeerRelay::iceAgent() const
{
  return mIceAgent;
}

int PeerRelay::port() const
{
  return mLocalGameUdpPort;
}

std::string const& PeerRelay::peerLogin() const
{
  return mPeerLogin;
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
  FAF_LOG_TRACE << "relaying " << size << " bytes from game to peer";
  if (mIceAgent &&
      mIceAgent->isConnected())
  {
    mIceAgent->send(std::string(buffer,
                                size));
  }
  else
  {
    FAF_LOG_ERROR << "mIceAgent not ready";
  }
  return true;
}
