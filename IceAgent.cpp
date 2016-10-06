#include "IceAgent.h"

#include <iostream>
#include <stdexcept>
#include <cstring>

#include <agent.h>
#include <nice.h>

void cb_candidate_gathering_done(NiceAgent *agent,
                                 guint stream_id,
                                 gpointer data)
{
  static_cast<IceAgent*>(data)->onCandidateGatheringDone();
}

void cb_component_state_changed(NiceAgent *agent,
                                guint stream_id,
                                guint component_id,
                                guint state,
                                gpointer data)
{
  static_cast<IceAgent*>(data)->onComponentStateChanged(state);
}

void cb_new_selected_pair_full(NiceAgent *agent,
                               guint stream_id,
                               guint component_id,
                               NiceCandidate* localCandidate,
                               NiceCandidate* remoteCandidate,
                               gpointer data)
{
  static_cast<IceAgent*>(data)->onCandidateSelected(
        localCandidate,
        remoteCandidate
        );
}

void cb_nice_recv(NiceAgent *agent,
                  guint stream_id,
                  guint component_id,
                  guint len,
                  gchar *buf,
                  gpointer data)
{
  static_cast<IceAgent*>(data)->onReceive(std::string(buf, len));
}

IceAgent::IceAgent(GMainLoop* mainloop,
                   bool controlling,
                   char* stunIp,
                   char* turnIp,
                   std::string const& turnUser,
                   std::string const& turnPassword):
  mAgent(nullptr),
  mTurnUser(turnUser),
  mTurnPassword(turnPassword),
  mSdp(nullptr),
  mSdp64(nullptr),
  mHasRemoteSdp(false),
  mConnected(false),
  mStreamId(0)
{
  mAgent = nice_agent_new(g_main_loop_get_context (mainloop),
                          NICE_COMPATIBILITY_RFC5245);
  if (mAgent == NULL)
  {
    throw std::runtime_error("nice_agent_new() failed");
  }

  g_object_set(mAgent, "stun-server", stunIp, nullptr);
  g_object_set(mAgent, "stun-server-port", 3478, nullptr);
  g_object_set(mAgent, "controlling-mode", controlling, nullptr);

  mStreamId = nice_agent_add_stream(mAgent, 1);
  if (mStreamId == 0)
  {
    throw std::runtime_error("nice_agent_add_stream() failed");
  }
  nice_agent_set_relay_info(mAgent,
                            mStreamId,
                            1,
                            turnIp,
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

  nice_agent_set_stream_name (mAgent,
                              mStreamId,
                              "application");

  nice_agent_attach_recv(mAgent,
                         mStreamId,
                         1,
                         g_main_loop_get_context(mainloop),
                         cb_nice_recv,
                         this);

  // Connect to the signals
  g_signal_connect(mAgent,
                   "candidate-gathering-done",
                    G_CALLBACK(cb_candidate_gathering_done),
                   this);
  g_signal_connect(mAgent,
                   "component-state-changed",
                    G_CALLBACK(cb_component_state_changed),
                   this);
  g_signal_connect(mAgent,
                   "new-selected-pair-full",
                   G_CALLBACK(cb_new_selected_pair_full),
                   this);
}

IceAgent::~IceAgent()
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
  if (mStreamId)
  {
    nice_agent_remove_stream(mAgent,
                             mStreamId);
  }
  if (mAgent)
  {
    g_object_unref(mAgent);
  }
}

void IceAgent::gatherCandidates()
{
  // Start gathering local candidates
  if (!nice_agent_gather_candidates(mAgent,
                                    mStreamId))
  {
    throw std::runtime_error("nice_agent_gather_candidates() failed");
  }
}

void IceAgent::setCandidateGatheringDoneCallback(CandidateGatheringDoneCallback cb)
{
  mGatheringDoneCallback = cb;
}

void IceAgent::setReceiveCallback(ReceiveCallback cb)
{
  mReceiveCallback = cb;
}

void IceAgent::setRemoteSdp(std::string const& sdpBase64)
{
  mRemoteSdp = sdpBase64;
  gsize sdp_len;
  char* sdp = reinterpret_cast<char*>(g_base64_decode(mRemoteSdp.c_str(), &sdp_len));
  mRemoteSdp = std::string(sdp, sdp_len);
  g_free (sdp);

  int res = nice_agent_parse_remote_sdp(mAgent, mRemoteSdp.c_str());
  if (res <= 0)
  {
    std::cerr << "res " << res << std::endl;
    throw std::runtime_error("nice_agent_parse_remote_sdp() failed");
  }
  mHasRemoteSdp = true;
}

bool IceAgent::hasRemoteSdp() const
{
  return mHasRemoteSdp;
}

bool IceAgent::isConnected() const
{
  return mConnected;
}

void IceAgent::send(std::string const& msg)
{
  if (isConnected())
  {
    nice_agent_send(mAgent,
                    mStreamId,
                    1,
                    msg.size(),
                    msg.c_str());
  }
}

std::string IceAgent::localCandidateInfo() const
{
  return mLocalCandidateInfo;
}

std::string IceAgent::remoteCandidateInfo() const
{
  return mRemoteCandidateInfo;
}

void IceAgent::onCandidateGatheringDone()
{
  mSdp = nice_agent_generate_local_sdp(mAgent);
  mSdp64 = g_base64_encode(reinterpret_cast<unsigned char *>(mSdp),
                           strlen(mSdp));
  if (mGatheringDoneCallback)
  {
    mGatheringDoneCallback(this, mSdp64);
  }
  //std::cout << "SDP:\n" << mSdp << std::endl;
}

void IceAgent::onComponentStateChanged(unsigned int state)
{
  switch (state)
  {
    case NICE_COMPONENT_STATE_DISCONNECTED:
      std::cerr << "NICE_COMPONENT_STATE_DISCONNECTED" << std::endl;
      mConnected = false;
      break;
    case NICE_COMPONENT_STATE_GATHERING:
      std::cout << "NICE_COMPONENT_STATE_GATHERING" << std::endl;
      mConnected = false;
      break;
    case NICE_COMPONENT_STATE_CONNECTING:
      std::cout << "NICE_COMPONENT_STATE_CONNECTING" << std::endl;
      mConnected = false;
      break;
    case NICE_COMPONENT_STATE_CONNECTED:
      std::cout << "NICE_COMPONENT_STATE_CONNECTED" << std::endl;
      mConnected = false;
      break;
    case NICE_COMPONENT_STATE_READY:
      std::cout << "NICE_COMPONENT_STATE_READY" << std::endl;
      mConnected = true;
      break;
    case NICE_COMPONENT_STATE_FAILED:
      std::cerr << "NICE_COMPONENT_STATE_FAILED" << std::endl;
      mConnected = false;
      break;
  }
}

std::string transportToString(NiceCandidateTransport t)
{
  switch (t)
  {
    case NICE_CANDIDATE_TRANSPORT_UDP:
      return "UDP";
    case NICE_CANDIDATE_TRANSPORT_TCP_ACTIVE:
      return "TCP_ACTIVE";
    case NICE_CANDIDATE_TRANSPORT_TCP_PASSIVE:
      return "TCP_PASSIVE";
    case NICE_CANDIDATE_TRANSPORT_TCP_SO:
      return "TCP_SO";
  }
}

std::string addrString(NiceAddress* a)
{
  char result[NICE_ADDRESS_STRING_LEN] = {0};
  nice_address_to_string (a, result);
  return std::string(result);
}

void IceAgent::onCandidateSelected(NiceCandidate* localCandidate,
                                    NiceCandidate* remoteCandidate)
{
  mLocalCandidateInfo += transportToString(localCandidate->transport);
  mLocalCandidateInfo += " ";
  mLocalCandidateInfo += addrString(&localCandidate->addr);
  mRemoteCandidateInfo += transportToString(remoteCandidate->transport);
  mRemoteCandidateInfo += " ";
  mRemoteCandidateInfo += addrString(&remoteCandidate->addr);
}

void IceAgent::onReceive(std::string const& msg)
{
  if (mReceiveCallback)
  {
    mReceiveCallback(this,
                     msg);
  }
}
