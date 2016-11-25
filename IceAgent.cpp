#include "IceAgent.h"

#include <stdexcept>
#include <cstring>

#include <agent.h>
#include <nice.h>

#include "IceAdapterOptions.h"
#include "logging.h"

std::string stateToString(IceAgentState const& s)
{
  switch(s)
  {
    case IceAgentState::NeedRemoteSdp:
      return "NeedRemoteSdp";
      break;
    case IceAgentState::Disconnected:
      return "Disconnected";
      break;
    case IceAgentState::Gathering:
      return "Gathering";
      break;
    case IceAgentState::Connecting:
      return "Connecting";
      break;
    case IceAgentState::Connected:
      return "Connected";
      break;
    case IceAgentState::Ready:
      return "Ready";
      break;
    case IceAgentState::Failed:
      return "Failed";
      break;
  }
}

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
                   std::string const& stunIp,
                   std::string const& turnIp,
                   IceAdapterOptions const& options):
  mAgent(nullptr),
  mTurnUser(options.turnUser),
  mTurnPassword(options.turnPass),
  mSdp(nullptr),
  mSdp64(nullptr),
  mHasRemoteSdp(false),
  mStreamId(0),
  mState(IceAgentState::NeedRemoteSdp)
{
  mAgent = nice_agent_new(g_main_loop_get_context (mainloop),
                          NICE_COMPATIBILITY_RFC5245);
  if (mAgent == NULL)
  {
    throw std::runtime_error("nice_agent_new() failed");
  }

  g_object_set(mAgent, "stun-server", stunIp.c_str(), nullptr);
  g_object_set(mAgent, "stun-server-port", 3478, nullptr);
  g_object_set(mAgent, "controlling-mode", controlling, nullptr);
  g_object_set(mAgent, "upnp", options.useUpnp, nullptr);

  mStreamId = nice_agent_add_stream(mAgent, 1);
  if (mStreamId == 0)
  {
    throw std::runtime_error("nice_agent_add_stream() failed");
  }
  nice_agent_set_relay_info(mAgent,
                            mStreamId,
                            1,
                            turnIp.c_str(),
                            3478,
                            mTurnUser.c_str(),
                            mTurnPassword.c_str(),
                            NICE_RELAY_TYPE_TURN_UDP);
  nice_agent_set_relay_info(mAgent,
                            mStreamId,
                            1,
                            turnIp.c_str(),
                            3478,
                            mTurnUser.c_str(),
                            mTurnPassword.c_str(),
                            NICE_RELAY_TYPE_TURN_TCP);

  if (options.iceLocalPortMin > 0)
  {
    int maxPort = options.iceLocalPortMin + 20;
    if (options.iceLocalPortMax > options.iceLocalPortMin)
    {
      maxPort = options.iceLocalPortMax;
    }
    nice_agent_set_port_range(mAgent,
                              mStreamId,
                              1,
                              static_cast<guint>(options.iceLocalPortMin),
                              static_cast<guint>(maxPort));
  }

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
  FAF_LOG_TRACE << "IceAgent()";
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
  FAF_LOG_TRACE << "~IceAgent()";
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

void IceAgent::setStateCallback(StateCallback cb)
{
  mStateCallback = cb;
}

void IceAgent::setRemoteSdp(std::string const& sdpBase64)
{
  mRemoteSdp64 = sdpBase64;
  gsize sdp_len;
  char* sdp = reinterpret_cast<char*>(g_base64_decode(mRemoteSdp64.c_str(), &sdp_len));
  mRemoteSdp64 = std::string(sdp, sdp_len);
  g_free (sdp);

  int res = nice_agent_parse_remote_sdp(mAgent, mRemoteSdp64.c_str());
  if (res <= 0)
  {
    FAF_LOG_ERROR << "res " << res;
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
  return mState == IceAgentState::Ready;
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

std::string IceAgent::localSdp() const
{
  if (mSdp)
  {
    return std::string(mSdp);
  }
  else
  {
    return std::string();
  }
}

std::string IceAgent::localSdp64() const
{
  if (mSdp64)
  {
    return std::string(mSdp64);
  }
  else
  {
    return std::string();
  }
}

std::string IceAgent::remoteSdp64() const
{
  return mRemoteSdp64;
}

IceAgentState IceAgent::state() const
{
  return mState;
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
  FAF_LOG_TRACE << "SDP:\n" << mSdp;
}

void IceAgent::onComponentStateChanged(unsigned int state)
{
  switch (state)
  {
    case NICE_COMPONENT_STATE_DISCONNECTED:
      BOOST_LOG_TRIVIAL(warning) << "NICE_COMPONENT_STATE_DISCONNECTED";
      mState = IceAgentState::Disconnected;
      break;
    case NICE_COMPONENT_STATE_GATHERING:
      FAF_LOG_TRACE << "NICE_COMPONENT_STATE_GATHERING";
      mState = IceAgentState::Gathering;
      break;
    case NICE_COMPONENT_STATE_CONNECTING:
      FAF_LOG_TRACE << "NICE_COMPONENT_STATE_CONNECTING";
      mState = IceAgentState::Connecting;
      break;
    case NICE_COMPONENT_STATE_CONNECTED:
      FAF_LOG_TRACE << "NICE_COMPONENT_STATE_CONNECTED";
      mState = IceAgentState::Connected;
      break;
    case NICE_COMPONENT_STATE_READY:
      FAF_LOG_TRACE << "NICE_COMPONENT_STATE_READY";
      mState = IceAgentState::Ready;
      break;
    case NICE_COMPONENT_STATE_FAILED:
      FAF_LOG_ERROR << "NICE_COMPONENT_STATE_FAILED";
      mState = IceAgentState::Failed;
      break;
  }
  if (mStateCallback)
  {
    mStateCallback(this, mState);
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
