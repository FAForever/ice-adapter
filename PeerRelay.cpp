#include "PeerRelay.h"

#include "IceAgent.h"
#include "logging.h"

PeerRelay::PeerRelay(Glib::RefPtr<Glib::MainLoop> mainloop,
                     int port,
                     int gamePort,
                     int peerId,
                     std::string const& stunIp,
                     std::string const& turnIp,
                     std::string const& turnUser,
                     std::string const& turnPassword):
  mMainloop(mainloop),
  mPeerId(peerId)
{
  mLocalGameUdpPort = port;
  mIceAgent = std::make_shared<IceAgent>(mainloop->gobj(),
                                         true,
                                         stunIp,
                                         turnIp,
                                         turnUser,
                                         turnPassword);

  mIceAgent->setReceiveCallback([this](IceAgent* agent, std::string const& message)
  {
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
        port);
  mLocalSocket->bind(srcAddress, false);

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
  FAF_LOG_TRACE << "onReceive " << size << " from " << socket_address_to_string(address) << ": " << buffer;
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
