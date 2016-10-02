#pragma once

#include <functional>

typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;

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
protected:
  void onCandidateGatheringDone();
  void onComponentStateChanged(unsigned int componentId,
                               unsigned int state);
  unsigned int mStreamId;
  IceAgent* mAgent;
  char* mSdp;
  char* mSdp64;

  CandidateGatheringDoneCallback mCb;

  friend void cb_candidate_gathering_done(NiceAgent*,
                                          unsigned int,
                                          void*);
  friend void cb_component_state_changed(NiceAgent *agent,
                                         unsigned int,
                                         unsigned int,
                                         unsigned int,
                                         void*);
};
