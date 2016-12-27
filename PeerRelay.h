#pragma once

#include <memory>
#include <functional>

#include <giomm.h>

#include "IceAdapterOptions.h"

namespace faf
{

/* Forward declarations */
class IceStream;
typedef std::shared_ptr<IceStream> IceStreamPtr;
enum class IceStreamState;

class PeerRelay
{
public:
  PeerRelay(IceStreamPtr iceStream,
            std::string const& peerLogin,
            IceAdapterOptions const& options
            );
  virtual ~PeerRelay();

  int localGameUdpPort() const;

  IceStreamPtr iceStream() const;

  std::string const& peerLogin() const;

  void reconnect();

  void sendPeerMessageToGame(std::string const& msg);

protected:
  bool onGameReceive(Glib::IOCondition);

  IceStreamPtr mIceStream;
  std::string mPeerLogin;
  Glib::RefPtr<Gio::Socket> mLocalSocket;
  int mLocalGameUdpPort;
  Glib::RefPtr<Gio::SocketAddress> mGameAddress;
  gchar mBuffer[4096];
};

}
