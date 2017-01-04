#include "IceAgent.h"

#include <stdexcept>
#include <cstring>

#include <glibmm.h>

#include <agent.h>
#include <nice.h>

#include "IceAdapterOptions.h"
#include "IceStream.h"
#include "logging.h"

#if SIGCXX_MAJOR_VERSION == 2 && SIGCXX_MICRO_VERSION == 0 && SIGCXX_MINOR_VERSION == 4
namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}
#endif

namespace faf
{

void cb_candidate_gathering_done(NiceAgent *agentHandle,
                                 guint stream_id,
                                 gpointer data)
{
  auto agent = static_cast<IceAgent*>(data);
  auto streamIt = agent->mSteamIdStreams.find(stream_id);
  if (streamIt == agent->mSteamIdStreams.end())
  {
    FAF_LOG_ERROR << "stream " << stream_id << " not found";
    return;
  }
  streamIt->second->onCandidateGatheringDone();
}

void cb_component_state_changed(NiceAgent *agentHandle,
                                guint stream_id,
                                guint component_id,
                                guint state,
                                gpointer data)
{
  auto agent = static_cast<IceAgent*>(data);
  auto streamIt = agent->mSteamIdStreams.find(stream_id);
  if (streamIt == agent->mSteamIdStreams.end())
  {
    FAF_LOG_ERROR << "stream " << stream_id << " not found";
    return;
  }
  streamIt->second->onComponentStateChanged(state);
}

void cb_new_selected_pair_full(NiceAgent *agentHandle,
                               guint stream_id,
                               guint component_id,
                               NiceCandidate* localCandidate,
                               NiceCandidate* remoteCandidate,
                               gpointer data)
{
  auto agent = static_cast<IceAgent*>(data);
  auto streamIt = agent->mSteamIdStreams.find(stream_id);
  if (streamIt == agent->mSteamIdStreams.end())
  {
    FAF_LOG_ERROR << "stream " << stream_id << " not found";
    return;
  }
  streamIt->second->onCandidateSelected(localCandidate,
                                 remoteCandidate);
}

void cb_new_candidate(NiceAgent *agentHandle,
                      NiceCandidate *candidate,
                      gpointer data)
{
  auto agent = static_cast<IceAgent*>(data);
  auto streamIt = agent->mSteamIdStreams.find(candidate->stream_id);
  if (streamIt == agent->mSteamIdStreams.end())
  {
    FAF_LOG_ERROR << "stream " << candidate->stream_id << " not found";
    return;
  }
  streamIt->second->onNewCandidate(candidate);
}

void  cb_stream_writeable(NiceAgent *agentHandle,
                          guint      stream_id,
                          guint      component_id,
                          gpointer   data)
{
  auto agent = static_cast<IceAgent*>(data);
  auto streamIt = agent->mSteamIdStreams.find(stream_id);
  if (streamIt == agent->mSteamIdStreams.end())
  {
    FAF_LOG_ERROR << "stream " << stream_id << " not found";
    return;
  }
  streamIt->second->mCanSend = true;
  FAF_LOG_INFO << streamIt->second->mPeerId << ": stream writeable";
}

IceAgent::IceAgent(GMainLoop* mainloop,
                   IceAdapterOptions const& options):
  mHandle(nullptr),
  mMainloop(mainloop),
  mOptions(options)
{
  mHandle = nice_agent_new_reliable(g_main_loop_get_context (mainloop),
                                    NICE_COMPATIBILITY_RFC5245);
  //mHandle = nice_agent_new(g_main_loop_get_context (mainloop),
  //                         NICE_COMPATIBILITY_RFC5245);
  if (mHandle == nullptr)
  {
    throw std::runtime_error("nice_agent_new() failed");
  }

  g_object_set(mHandle, "stun-server", mOptions.stunIp.c_str(), nullptr);
  g_object_set(mHandle, "stun-server-port", 3478, nullptr);
  g_object_set(mHandle, "controlling-mode", true, nullptr);
  g_object_set(mHandle, "ice-tcp", true, nullptr);
  g_object_set(mHandle, "ice-udp", true, nullptr);
  g_object_set(mHandle, "upnp", mOptions.useUpnp, nullptr);


  // Connect to the signals
  g_signal_connect(mHandle,
                   "candidate-gathering-done",
                    G_CALLBACK(cb_candidate_gathering_done),
                   this);
  g_signal_connect(mHandle,
                   "component-state-changed",
                    G_CALLBACK(cb_component_state_changed),
                   this);
  g_signal_connect(mHandle,
                   "new-selected-pair-full",
                   G_CALLBACK(cb_new_selected_pair_full),
                   this);
  g_signal_connect(mHandle,
                   "new-candidate-full",
                   G_CALLBACK(cb_new_candidate),
                   this);
  g_signal_connect(mHandle,
                   "reliable-transport-writable",
                   G_CALLBACK(cb_stream_writeable),
                   this);

}

IceAgent::~IceAgent()
{
  if (mHandle)
  {
    g_object_unref(mHandle);
  }
}

NiceAgent* IceAgent::handle() const
{
  return mHandle;
}

IceStreamPtr IceAgent::createStream(int peerId,
                                    IceStream::SdpCallback sdpCallback,
                                    IceStream::ReceiveCallback receiveCallback,
                                    IceStream::StateCallback stateCallback,
                                    IceStream::CandidateSelectedCallback candidateSelectedCallback,
                                    IceStream::ConnectivityChangedCallback connectivityChangedCallback)
{
  auto result = std::make_shared<IceStream>(shared_from_this(),
                                            peerId,
                                            sdpCallback,
                                            receiveCallback,
                                            stateCallback,
                                            candidateSelectedCallback,
                                            connectivityChangedCallback,
                                            mOptions);
  if (mPeerStreams.count(peerId) > 0)
  {
    FAF_LOG_ERROR << "IceStream for peer " << peerId << " already exists";
    removeStream(peerId);
  }
  mPeerStreams[peerId] = result;
  mSteamIdStreams[result->streamId()] = result;
  return result;
}

void IceAgent::removeStream(int peerId)
{
  auto streamIt = mPeerStreams.find(peerId);
  if (streamIt == mPeerStreams.end())
  {
    FAF_LOG_ERROR << "IceStream for peer " << peerId << " not found";
    return;
  }
  mSteamIdStreams.erase(streamIt->second->streamId());
  mPeerStreams.erase(streamIt);
}

void IceAgent::clear()
{
  mSteamIdStreams.clear();
  mPeerStreams.clear();
}

GMainLoop* IceAgent::mainloop() const
{
  return mMainloop;
}

}
