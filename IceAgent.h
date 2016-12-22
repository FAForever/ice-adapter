#pragma once

#include <functional>
#include <map>

#include <sigc++/sigc++.h>

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;
typedef struct _NiceCandidate NiceCandidate;

namespace faf
{

class IceAdapterOptions;

enum class IceAgentState
{
  NeedRemoteSdp,
  Disconnected,
  Gathering,
  Connecting,
  Connected,
  Ready,
  Failed
};

std::string stateToString(IceAgentState const& s);

class IceAgent
{
public:
  IceAgent(GMainLoop* mainloop,
           bool offering,
           int peerId,
           std::string const& stunIp,
           std::string const& turnIp,
           IceAdapterOptions const& options);
  virtual ~IceAgent();

  void gatherCandidates();

  typedef std::function<void (IceAgent*, std::string const&, std::string const&)> SdpMessageCallback;
  void setSdpMessageCallback(SdpMessageCallback cb);

  typedef std::function<void (IceAgent*, std::string const&)> ReceiveCallback;
  void setReceiveCallback(ReceiveCallback cb);

  typedef std::function<void (IceAgent*, IceAgentState const&)> StateCallback;
  void setStateCallback(StateCallback cb);

  typedef std::function<void (IceAgent*, std::string const&, std::string const&)> CandidateSelectedCallback;
  void setCandidateSelectedCallback(CandidateSelectedCallback cb);

  sigc::signal<void> onPeerConnectedToMe;

  void addRemoteSdpMessage(std::string const& type, std::string const& msg);
  bool hasRemoteSdp() const;
  void send(std::string const& msg);

  std::string localCandidateInfo() const;
  std::string remoteCandidateInfo() const;
  std::string remoteSdp() const;

  IceAgentState state() const;

  double timeToConnected() const;

  bool peerConnectedToMe() const;
  bool connectedToPeer() const;

protected:
  void onCandidateGatheringDone();
  void onComponentStateChanged(unsigned int state);
  void onCandidateSelected(NiceCandidate* localCandidate,
                           NiceCandidate* remoteCandidate);
  void onNewCandidate(NiceCandidate* localCandidate);
  void onReceive(std::string const& msg);
  bool pingPeer();

  NiceAgent* mAgent;
  std::string mTurnUser;
  std::string mTurnPassword;
  bool mHasRemoteSdp;
  bool mConnectedToPeer;
  bool mPeerConnectedToMe;
  bool mOffering;
  bool mPingPeer;
  int mPeerId;
  std::string mLocalCandidateInfo;
  std::string mRemoteCandidateInfo;
  std::string mRemoteSdp;

  SdpMessageCallback mSdpMessageCallback;
  ReceiveCallback mReceiveCallback;
  StateCallback mStateCallback;
  CandidateSelectedCallback mCandidateSelectedCallback;

  unsigned int mStreamId;

  IceAgentState mState;

  int64_t mStartTime;
  int64_t mConnectedTime;

  sigc::connection mTimerConnection;

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
  friend void cb_nice_recv(NiceAgent*,
                           unsigned int,
                           unsigned int,
                           unsigned int,
                           char*,
                           void*);
  friend void cb_new_candidate(NiceAgent*,
                               NiceCandidate*,
                               void*);
};

}
