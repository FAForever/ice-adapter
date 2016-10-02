#pragma once


#include <map>


typedef struct _NiceAgent NiceAgent;
typedef struct _GMainLoop GMainLoop;

class IceStream;

class IceAgent
{
public:
  IceAgent(GMainLoop* mainloop,
           std::string const& stunTurnHost);
  virtual ~IceAgent();

  IceStream* createStream();

  NiceAgent* agent() const;
protected:
  NiceAgent* mAgent;
  GMainLoop* mMainloop;
  std::map<unsigned int, IceStream*> mStreams;

  friend void cb_candidate_gathering_done(NiceAgent*,
                                          unsigned int,
                                          void*);
  friend void cb_component_state_changed(NiceAgent *agent,
                                         unsigned int,
                                         unsigned int,
                                         unsigned int,
                                         void*);
};
