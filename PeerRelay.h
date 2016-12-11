#pragma once

#include <memory>
#include <functional>

#include <giomm.h>

#include "IceAdapterOptions.h"

namespace faf
{

/* Forward declarations */
class IceAgent;
enum class IceAgentState;

class PeerRelay
{
public:
  typedef std::function<void (PeerRelay*, std::string const&, std::string const&)> SdpMessageCallback;
  typedef std::function<void (PeerRelay*, IceAgentState const&)> IceAgentStateCallback;
  typedef std::function<void (PeerRelay*, std::string const&, std::string const&)> CandidateSelectedCallback;

  PeerRelay(Glib::RefPtr<Glib::MainLoop> mainloop,
            int peerId,
            std::string const& peerLogin,
            std::string const& stunIp,
            std::string const& turnIp,
            SdpMessageCallback sdpCb,
            IceAgentStateCallback stateCb,
            CandidateSelectedCallback candSelCb,
            bool createOffer,
            IceAdapterOptions const& options
            );
  virtual ~PeerRelay();

  void setPeer(int peerId,
               std::string const& peerLogin);

  int peerId() const;

  int localGameUdpPort() const;

  std::shared_ptr<IceAgent> iceAgent() const;

  std::string const& peerLogin() const;

  void reconnect();

  bool isOfferer() const;

protected:
  void createAgent();
  bool onGameReceive(Glib::IOCondition condition);
  Glib::RefPtr<Glib::MainLoop> mMainloop;
  Glib::RefPtr<Gio::Socket> mLocalSocket;
  int mPeerId;
  std::string mPeerLogin;
  std::string mStunIp;
  std::string mTurnIp;
  int mLocalGameUdpPort;
  bool mCreateOffer;
  std::shared_ptr<IceAgent> mIceAgent;
  Glib::RefPtr<Gio::SocketAddress> mGameAddress;
  SdpMessageCallback mSdpMessageCallback;
  IceAgentStateCallback mIceAgentStateCallback;
  CandidateSelectedCallback mCandidateSelectedCallback;
  IceAdapterOptions mOptions;
};

}
