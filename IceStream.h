#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include <string>

#include <sigc++/sigc++.h>

typedef struct _NiceAgent NiceAgent;
typedef struct _NiceCandidate NiceCandidate;

namespace faf {

class IceAdapterOptions;
class IceAgent;
typedef std::shared_ptr<IceAgent> IceAgentPtr;

enum class IceStreamState
{
  NeedRemoteSdp,
  Disconnected,
  Gathering,
  Connecting,
  Connected,
  Ready,
  Failed
};

std::string stateToString(IceStreamState const& s);

class IceStream
{
public:
  IceStream(IceAgentPtr agent,
            int peerId,
            std::string const& turnIp,
            IceAdapterOptions const& options);
  virtual ~IceStream();

  unsigned int streamId() const;

  void gatherCandidates();

  typedef std::function<void (IceStream*, std::string const&)> SdpCallback;
  void setSdpCallback(SdpCallback cb);

  typedef std::function<void (IceStream*, std::string const&)> ReceiveCallback;
  void setReceiveCallback(ReceiveCallback cb);

  typedef std::function<void (IceStream*, IceStreamState const&)> StateCallback;
  void setStateCallback(StateCallback cb);

  typedef std::function<void (IceStream*, std::string const&, std::string const&)> CandidateSelectedCallback;
  void setCandidateSelectedCallback(CandidateSelectedCallback cb);

  typedef std::function<void (IceStream*, bool, bool)> ConnectivityChangedCallback;
  void setConnectivityChangedCallback(ConnectivityChangedCallback cb);

  void addRemoteSdp(std::string const& sdp);
  void send(std::string const& msg);

  std::string localCandidateInfo() const;
  std::string remoteCandidateInfo() const;
  std::string remoteSdp() const;

  IceStreamState state() const;

  double timeToConnected() const;

  bool peerConnectedToMe() const;
  bool connectedToPeer() const;

protected:
  void setConnectivity(bool connectedToPeer, bool peerConnectedToMe);
  void onReceive(std::string const& msg);
  void onCandidateGatheringDone();
  void onComponentStateChanged(unsigned int state);
  void onCandidateSelected(NiceCandidate* localCandidate,
                           NiceCandidate* remoteCandidate);
  void onNewCandidate(NiceCandidate* localCandidate);
  bool pingPeer();

  IceAgentPtr mAgent;
  int mPeerId;

  unsigned int mStreamId;
  IceStreamState mState;
  int64_t mStartTime;
  int64_t mConnectedTime;
  bool mPingPeer;
  bool mConnectedToPeer;
  bool mPeerConnectedToMe;
  bool mLocalCandidatesGathered;
  std::string mLocalCandidateInfo;
  std::string mRemoteCandidateInfo;
  std::string mRemoteSdp;

  SdpCallback mSdpCallback;
  ReceiveCallback mReceiveCallback;
  StateCallback mStateCallback;
  CandidateSelectedCallback mCandidateSelectedCallback;
  ConnectivityChangedCallback mConnectivityChangedCallback;

  sigc::connection mTimerConnection;
  sigc::connection mTurnTimerConnection;

  friend void cb_candidate_gathering_done(NiceAgent*,
                                          unsigned int,
                                          void*);
  friend void cb_component_state_changed(NiceAgent *agent,
                                         unsigned int,
                                         unsigned int,
                                         unsigned int,
                                         void*);
  friend void cb_new_selected_pair_full(NiceAgent*,
                                        unsigned int,
                                        unsigned int,
                                        NiceCandidate*,
                                        NiceCandidate*,
                                        void*);
  friend void cb_new_candidate(NiceAgent*,
                               NiceCandidate*,
                               void*);
  friend void cb_nice_recv(NiceAgent*,
                           unsigned int,
                           unsigned int,
                           unsigned int,
                           char*,
                           void*);
};

typedef std::shared_ptr<IceStream> IceStreamPtr;

} // namespace faf
