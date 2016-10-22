#include "PeerRelay.h"

#include <boost/log/trivial.hpp>

#include "IceAgent.h"

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

}

void PeerRelay::gatherCandidates(CandidateGatheringDoneCallback cb)
{
  mIceAgent->setCandidateGatheringDoneCallback([this, cb](IceAgent* agent, std::string const& sdp)
  {
    cb(this, sdp);
  });
  mIceAgent->gatherCandidates();
}

bool PeerRelay::onGameReceive(Glib::IOCondition condition)
{
  gchar buffer[4096] = {};
  Glib::RefPtr<Gio::SocketAddress> address;
  //auto address = mLocalSocket->get_remote_address();
  auto size = mLocalSocket->receive_from(address,
                                         buffer,
                                         4096);
  BOOST_LOG_TRIVIAL(trace) << "onReceive " << size << " from " << address->to_string() << ": " << buffer;
  if (mIceAgent &&
      mIceAgent->isConnected())
  {
    mIceAgent->send(std::string(buffer,
                                size));
  }
  else
  {
    BOOST_LOG_TRIVIAL(error) << "mIceAgent not ready";
  }
  return true;
}
