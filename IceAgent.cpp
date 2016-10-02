#include "IceAgent.h"

#include <iostream>
#include <stdexcept>

#include <agent.h>

#include "IceStream.h"

void
cb_candidate_gathering_done(NiceAgent *agent,
                            guint stream_id,
                            gpointer data)
{
  auto a = static_cast<IceAgent*>(data);
  auto i = a->mStreams.find(stream_id);
  if (i != a->mStreams.end())
  {
    i->second->onCandidateGatheringDone();
  }
  else
  {
    std::cerr << "stream ID " << stream_id << " not found" << std::endl;
  }
}

void
cb_component_state_changed(NiceAgent *agent,
                           guint stream_id,
                           guint component_id,
                           guint state,
                           gpointer data)
{
  auto a = static_cast<IceAgent*>(data);
  auto i = a->mStreams.find(stream_id);
  if (i != a->mStreams.end())
  {
    i->second->onComponentStateChanged(component_id,
                                       state);
  }
}

IceAgent::IceAgent(GMainLoop* mainloop,
                   std::string const& stunTurnHost):
  mAgent(nullptr),
  mMainloop(mainloop)
{
  mAgent = nice_agent_new(g_main_loop_get_context (mainloop),
                          NICE_COMPATIBILITY_RFC5245);
  if (mAgent == NULL)
  {
    throw std::runtime_error("nice_agent_new() failed");
  }

  auto resolver = g_resolver_get_default();

  auto results = g_resolver_lookup_by_name(resolver,
                                           stunTurnHost.c_str(),
                                           nullptr,
                                           nullptr);
  auto addr = G_INET_ADDRESS (g_object_ref (results->data));

  g_resolver_free_addresses (results);
  g_object_unref (resolver);

  g_object_set(mAgent, "stun-server", addr, nullptr);
  g_object_set(mAgent, "stun-server-port", 3478, nullptr);
  g_object_set(mAgent, "controlling-mode", true, nullptr);

  g_object_unref (addr);

  // Connect to the signals
  g_signal_connect(mAgent,
                   "candidate-gathering-done",
                    G_CALLBACK(cb_candidate_gathering_done),
                   this);
  g_signal_connect(mAgent,
                   "component-state-changed",
                    G_CALLBACK(cb_component_state_changed),
                   this);
}

IceAgent::~IceAgent()
{
  if (mAgent)
  {
    g_object_unref(mAgent);
  }
}

IceStream* IceAgent::createStream()
{
  guint streamId = nice_agent_add_stream(mAgent, 1);
  if (streamId == 0)
  {
    throw std::runtime_error("nice_agent_add_stream() failed");
  }

  IceStream* result = new IceStream(streamId,
                                    this,
                                    mMainloop);
  mStreams.insert(std::make_pair(streamId, result));
  return result;
}

NiceAgent* IceAgent::agent() const
{
  return mAgent;
}
