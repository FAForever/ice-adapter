#pragma once

#include <memory>
#include <functional>

#include <giomm.h>

#include "IceAdapterOptions.h"

/* Forward declarations */
class IceAgent;
enum class IceAgentState;

class PeerRelay
{
public:
  typedef std::function<void (PeerRelay*, std::string const&)> CandidateGatheringDoneCallback;
  typedef std::function<void (PeerRelay*, IceAgentState const&)> IceAgentStateCallback;

  PeerRelay(Glib::RefPtr<Glib::MainLoop> mainloop,
            int peerId,
            std::string const& peerLogin,
            std::string const& stunIp,
            std::string const& turnIp,
            CandidateGatheringDoneCallback gatherDoneCb,
            IceAgentStateCallback stateCb,
            IceAdapterOptions const& options
            );
  virtual ~PeerRelay();

  int localGameUdpPort() const;

  std::shared_ptr<IceAgent> iceAgent() const;

  int port() const;

  std::string const& peerLogin() const;

  void reconnect();

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
  std::shared_ptr<IceAgent> mIceAgent;
  Glib::RefPtr<Gio::SocketAddress> mGameAddress;
  CandidateGatheringDoneCallback mCandidateGatheringDoneCallback;
  IceAgentStateCallback mIceAgentStateCallback;
  IceAdapterOptions mOptions;
};
