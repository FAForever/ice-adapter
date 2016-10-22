#pragma once

#include <memory>
#include <functional>

#include <giomm.h>

class IceAgent;

class PeerRelay
{
public:
  PeerRelay(Glib::RefPtr<Glib::MainLoop> mainloop,
            int port,
            int gamePort,
            int peerId,
            std::string const& stunIp,
            std::string const& turnIp,
            std::string const& turnUser,
            std::string const& turnPassword);

  typedef std::function<void (PeerRelay*, std::string const&)> CandidateGatheringDoneCallback;
  void gatherCandidates(CandidateGatheringDoneCallback cb);

protected:
  bool onGameReceive(Glib::IOCondition condition);
  Glib::RefPtr<Glib::MainLoop> mMainloop;
  Glib::RefPtr<Gio::Socket> mLocalSocket;
  int mPeerId;
  std::shared_ptr<IceAgent> mIceAgent;
  Glib::RefPtr<Gio::SocketAddress> mGameAddress;
};
