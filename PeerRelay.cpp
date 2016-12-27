#include "PeerRelay.h"

#include "IceStream.h"
#include "logging.h"

namespace faf
{

PeerRelay::PeerRelay(IceStreamPtr iceStream,
                     std::string const& peerLogin,
                     IceAdapterOptions const& options):
  mIceStream(iceStream),
  mPeerLogin(peerLogin),
  mLocalGameUdpPort(0)
{
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
    FAF_LOG_DEBUG << "PeerRelay for player " << mIceStream->peerId() << " listening on port " << mLocalGameUdpPort;
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
                                                static_cast<guint16>(options.gameUdpPort));

  FAF_LOG_TRACE << "PeerRelay " << mIceStream->peerId() << " constructed";
}

PeerRelay::~PeerRelay()
{
  FAF_LOG_TRACE << "PeerRelay " << mIceStream->peerId() << " destructed";
}

int PeerRelay::localGameUdpPort() const
{
  return mLocalGameUdpPort;
}

IceStreamPtr PeerRelay::iceStream() const
{
  return mIceStream;
}

std::string const& PeerRelay::peerLogin() const
{
  return mPeerLogin;
}

void PeerRelay::reconnect()
{
  FAF_LOG_ERROR << "implementme";
}

void PeerRelay::sendPeerMessageToGame(std::string const& msg)
{
  try
  {
    mLocalSocket->send_to(mGameAddress,
                          msg.c_str(),
                          msg.size());

  }
  catch (const Glib::Exception& e)
  {
    FAF_LOG_ERROR << "error sending " << msg.size() << " bytes to game: " << e.what();
  }
  catch (const std::exception& e)
  {
    FAF_LOG_ERROR << "error sending " << msg.size() << " bytes to game: " << e.what();
  }
  catch (...)
  {
    FAF_LOG_ERROR << "unknown error occured";
  }
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

bool PeerRelay::onGameReceive(Glib::IOCondition)
{
  Glib::RefPtr<Gio::SocketAddress> address;
  //auto address = mLocalSocket->get_remote_address();
  auto size = mLocalSocket->receive_from(address,
                                         mBuffer,
                                         4096);
  if (size > 0)
  {
    mIceStream->send(std::string(mBuffer,
                                 size));
  }

  return true;
}

}
