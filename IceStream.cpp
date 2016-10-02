#include "IceStream.h"

#include <iostream>

#include <cstring>

#include <agent.h>

#include "IceAgent.h"

void
cb_nice_recv(NiceAgent *agent, guint stream_id, guint component_id,
    guint len, gchar *buf, gpointer data)
{
}

IceStream::IceStream(unsigned int streamId,
                     IceAgent* agent,
                     GMainLoop* mainloop):
  mStreamId(streamId),
  mAgent(agent),
  mSdp(nullptr),
  mSdp64(nullptr)
{
  nice_agent_set_stream_name (agent->agent(),
                              streamId,
                              "application");

  nice_agent_attach_recv(agent->agent(),
                         streamId,
                         1,
                         g_main_loop_get_context(mainloop),
                         cb_nice_recv,
                         this);
}

IceStream::~IceStream()
{
  if (mSdp)
  {
    g_free(mSdp);
    mSdp = nullptr;
  }
  if (mSdp64)
  {
    g_free(mSdp64);
    mSdp64 = nullptr;
  }
}

void IceStream::gatherCandidates()
{
  // Start gathering local candidates
  if (!nice_agent_gather_candidates(mAgent->agent(),
                                    mStreamId))
  {
    throw std::runtime_error("nice_agent_gather_candidates() failed");
  }
}

void IceStream::setCandidateGatheringDoneCallback(CandidateGatheringDoneCallback cb)
{
  mCb = cb;
}

void IceStream::onCandidateGatheringDone()
{
  mSdp = nice_agent_generate_local_sdp(mAgent->agent());
  mSdp64 = g_base64_encode(reinterpret_cast<unsigned char *>(mSdp),
                           strlen(mSdp));
  if (mCb)
  {
    mCb(this, mSdp64);
  }
  std::cout << "SDP:\n" << mSdp << std::endl;
}

void IceStream::onComponentStateChanged(unsigned int componentId,
                                        unsigned int state)
{

}
