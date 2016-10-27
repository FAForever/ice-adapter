#pragma once

#include <functional>
#include <map>

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;
typedef struct _NiceCandidate NiceCandidate;

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
           bool controlling,
           std::string const& stunIp,
           std::string const& turnIp,
           std::string const& turnUser,
           std::string const& turnPassword);
  virtual ~IceAgent();

  void gatherCandidates();

  typedef std::function<void (IceAgent*, std::string const&)> CandidateGatheringDoneCallback;
  void setCandidateGatheringDoneCallback(CandidateGatheringDoneCallback cb);

  typedef std::function<void (IceAgent*, std::string const&)> ReceiveCallback;
  void setReceiveCallback(ReceiveCallback cb);

  typedef std::function<void (IceAgent*, IceAgentState const&)> StateCallback;
  void setStateCallback(StateCallback cb);

  void setRemoteSdp(std::string const& sdpBase64);
  bool hasRemoteSdp() const;
  bool isConnected() const;
  void send(std::string const& msg);

  std::string localCandidateInfo() const;
  std::string remoteCandidateInfo() const;

  std::string localSdp() const;
  std::string localSdp64() const;
  std::string remoteSdp64() const;

  IceAgentState state() const;

protected:
  void onCandidateGatheringDone();
  void onComponentStateChanged(unsigned int state);
  void onCandidateSelected(NiceCandidate* localCandidate,
                           NiceCandidate* remoteCandidate);
  void onReceive(std::string const& msg);

  NiceAgent* mAgent;
  std::string mTurnUser;
  std::string mTurnPassword;
  char* mSdp;
  char* mSdp64;
  bool mHasRemoteSdp;
  bool mConnected;
  std::string mLocalCandidateInfo;
  std::string mRemoteCandidateInfo;
  std::string mRemoteSdp64;

  CandidateGatheringDoneCallback mGatheringDoneCallback;
  ReceiveCallback mReceiveCallback;
  StateCallback mStateCallback;

  unsigned int mStreamId;

  IceAgentState mState;

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
};
