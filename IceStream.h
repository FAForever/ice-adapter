#pragma once

#include <functional>

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;
typedef struct _NiceCandidate NiceCandidate;

class IceAgent;

class IceStream
{
public:
  IceStream(unsigned int streamId,
            IceAgent* agent,
            GMainLoop* mainloop);
  virtual ~IceStream();
  void gatherCandidates();

  typedef std::function<void (IceStream*, std::string const&)> CandidateGatheringDoneCallback;
  void setCandidateGatheringDoneCallback(CandidateGatheringDoneCallback cb);

  typedef std::function<void (IceStream*, std::string const&)> ReceiveCallback;
  void setReceiveCallback(ReceiveCallback cb);

  void setRemoteSdp(std::string const& sdpBase64);
  bool hasRemoteSdp() const;
  bool isConnected() const;
  void send(std::string const& msg);
protected:
  void onCandidateGatheringDone();
  void onComponentStateChanged(unsigned int componentId,
                               unsigned int state);
  void onCandidateSelected(NiceCandidate* localCandidate,
                           NiceCandidate* remoteCandidate);
  void onReceive(std::string const& msg);
  unsigned int mStreamId;
  IceAgent* mAgent;
  char* mSdp;
  char* mSdp64;
  bool mHasRemoteSdp;
  bool mConnected;

  CandidateGatheringDoneCallback mCb;
  ReceiveCallback mReceiveCallback;

  friend void cb_candidate_gathering_done(NiceAgent*,
                                          unsigned int,
                                          void*);
  friend void cb_component_state_changed(NiceAgent*,
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
