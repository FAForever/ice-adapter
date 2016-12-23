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
  typedef std::function<void (PeerRelay*, std::string const&)> SdpCallback;
  typedef std::function<void (PeerRelay*, IceStreamState const&)> IceStreamStateCallback;
  typedef std::function<void (PeerRelay*, std::string const&, std::string const&)> CandidateSelectedCallback;
  typedef std::function<void (PeerRelay*, bool, bool)> ConnectivityChangedCallback;

  PeerRelay(IceStreamPtr iceStream,
            int peerId,
            std::string const& peerLogin,
            std::string const& stunIp,
            std::string const& turnIp,
            SdpCallback sdpCb,
            IceStreamStateCallback stateCb,
            CandidateSelectedCallback candSelCb,
            ConnectivityChangedCallback connCb,
            IceAdapterOptions const& options
            );
  virtual ~PeerRelay();

  int peerId() const;

  int localGameUdpPort() const;

  IceStreamPtr iceStream() const;

  std::string const& peerLogin() const;

  void reconnect();

  bool isOfferer() const;

protected:
  bool onGameReceive(Glib::IOCondition);

  IceStreamPtr mIceStream;
  int mPeerId;
  std::string mPeerLogin;
  std::string mStunIp;
  std::string mTurnIp;
  SdpCallback mSdpCallback;
  IceStreamStateCallback mIceStreamStateCallback;
  CandidateSelectedCallback mCandidateSelectedCallback;
  ConnectivityChangedCallback mConnectivityChangedCallback;
  Glib::RefPtr<Gio::Socket> mLocalSocket;
  int mLocalGameUdpPort;
  Glib::RefPtr<Gio::SocketAddress> mGameAddress;
  gchar mBuffer[4096];
};

}
