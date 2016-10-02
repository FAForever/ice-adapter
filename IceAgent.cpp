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
                   std::string const& stunHost,
                   std::string const& turnHost,
                   std::string const& turnUser,
                   std::string const& turnPassword):
  mAgent(nullptr),
  mMainloop(mainloop),
  mStunIp(nullptr),
  mTurnIp(nullptr),
  mTurnUser(turnUser),
  mTurnPassword(turnPassword)
{
  mAgent = nice_agent_new(g_main_loop_get_context (mainloop),
                          NICE_COMPATIBILITY_RFC5245);
  if (mAgent == NULL)
  {
    throw std::runtime_error("nice_agent_new() failed");
  }

  auto resolver = g_resolver_get_default();

  auto stunResults = g_resolver_lookup_by_name(resolver,
                                           stunHost.c_str(),
                                           nullptr,
                                           nullptr);
  auto stunAddr = G_INET_ADDRESS (g_object_ref (stunResults->data));
  mStunIp = g_inet_address_to_string(stunAddr);
  g_object_unref(stunAddr);

  g_resolver_free_addresses (stunResults);

  auto turnResults = g_resolver_lookup_by_name(resolver,
                                               turnHost.c_str(),
                                               nullptr,
                                               nullptr);
  auto turnAddr = G_INET_ADDRESS (g_object_ref (turnResults->data));
  mTurnIp = g_inet_address_to_string(turnAddr);
  g_object_unref(turnAddr);

  g_object_unref (resolver);

  g_object_set(mAgent, "stun-server", mStunIp, nullptr);
  g_object_set(mAgent, "stun-server-port", 3478, nullptr);
  g_object_set(mAgent, "controlling-mode", true, nullptr);


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
  if (mStunIp)
  {
    g_free(mStunIp);
  }
  if (mTurnIp)
  {
    g_free(mTurnIp);
  }
}

IceStream* IceAgent::createStream()
{
  guint streamId = nice_agent_add_stream(mAgent, 1);
  if (streamId == 0)
  {
    throw std::runtime_error("nice_agent_add_stream() failed");
  }
  nice_agent_set_relay_info(mAgent,
                            streamId,
                            1,
                            mTurnIp,
                            3478,
                            mTurnUser.c_str(),
                            mTurnPassword.c_str(),
                            NICE_RELAY_TYPE_TURN_UDP);
  /* crashes?
  nice_agent_set_relay_info(mAgent,
                            streamId,
                            1,
                            mStunTurnIp,
                            3478,
                            "mm+viagenie.ca@netlair.de",
                            "asdf",
                            NICE_RELAY_TYPE_TURN_TCP);
                            */

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
