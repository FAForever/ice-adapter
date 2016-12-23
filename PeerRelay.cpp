#include "PeerRelay.h"

#include "IceStream.h"
#include "logging.h"

namespace faf
{

PeerRelay::PeerRelay(IceStreamPtr iceStream,
                     int peerId,
                     std::string const& peerLogin,
                     std::string const& stunIp,
                     std::string const& turnIp,
                     SdpCallback sdpCb,
                     IceStreamStateCallback stateCb,
                     CandidateSelectedCallback candSelCb,
                     ConnectivityChangedCallback connCb,
                     IceAdapterOptions const& options):
  mIceStream(iceStream),
  mPeerId(peerId),
  mPeerLogin(peerLogin),
  mStunIp(stunIp),
  mTurnIp(turnIp),
  mSdpCallback(sdpCb),
  mIceStreamStateCallback(stateCb),
  mCandidateSelectedCallback(candSelCb),
  mConnectivityChangedCallback(connCb),
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
    FAF_LOG_DEBUG << "PeerRelay for player " << peerId << " listening on port " << mLocalGameUdpPort;
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

  mIceStream->setReceiveCallback([this](IceStream*, std::string const& message)
  {
    try
    {
      mLocalSocket->send_to(mGameAddress,
                            message.c_str(),
                            message.size());

    }
    catch (const Glib::Exception& e)
    {
      FAF_LOG_ERROR << "error sending " << message.size() << " bytes to game: " << e.what();
    }
    catch (const std::exception& e)
    {
      FAF_LOG_ERROR << "error sending " << message.size() << " bytes to game: " << e.what();
    }
    catch (...)
    {
      FAF_LOG_ERROR << "unknown error occured";
    }
  });

  mIceStream->setStateCallback([this](IceStream*, IceStreamState const& state)
  {
    mIceStreamStateCallback(this, state);
  });

  mIceStream->setSdpCallback([this](IceStream*, std::string const& sdp)
  {
    mSdpCallback(this, sdp);
  });

  mIceStream->setCandidateSelectedCallback([this](IceStream*, std::string const& local, std::string const& remote)
  {
    mCandidateSelectedCallback(this, local, remote);
  });

  mIceStream->setConnectivityChangedCallback([this](IceStream*, bool connectedToPeer, bool peerConnectedToMe)
  {
    mConnectivityChangedCallback(this, connectedToPeer, peerConnectedToMe);
  });

  FAF_LOG_TRACE << "PeerRelay " << mPeerId << " constructed";
}

PeerRelay::~PeerRelay()
{
  FAF_LOG_TRACE << "PeerRelay " << mPeerId << " destructed";
}

int PeerRelay::peerId() const
{
  return mPeerId;
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
  mIceStream->send(std::string(mBuffer,
                               size));

  return true;
}

}
