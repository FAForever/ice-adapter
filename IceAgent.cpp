#include "IceAgent.h"

#include <boost/preprocessor/stringize.hpp>

#include <stdexcept>
#include <cstring>

#include <glibmm.h>

#include <agent.h>
#include <nice.h>

#include "IceAdapterOptions.h"
#include "logging.h"

#if SIGCXX_MAJOR_VERSION == 2 && SIGCXX_MICRO_VERSION == 0 && SIGCXX_MINOR_VERSION == 4
namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}
#endif

namespace faf
{

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

void cb_new_candidate(NiceAgent *agent,
                      NiceCandidate *candidate,
                      gpointer data)
{
  static_cast<IceAgent*>(data)->onNewCandidate(candidate);
}

void cb_nice_recv(NiceAgent *agent,
                  guint stream_id,
                  guint component_id,
                  guint len,
                  gchar *buf,
                  gpointer data)
{
  if (component_id == 2)
  {
    FAF_LOG_WARN << "component_id == 2";
    return;
  }
  static_cast<IceAgent*>(data)->onReceive(std::string(buf, len));
}

IceAgent::IceAgent(GMainLoop* mainloop,
                   bool offering,
                   int peerId,
                   std::string const& stunIp,
                   std::string const& turnIp,
                   IceAdapterOptions const& options):
  mAgent(nullptr),
  mTurnUser(options.turnUser),
  mTurnPassword(options.turnPass),
  mHasRemoteSdp(false),
  mConnected(false),
  mOffering(offering),
  mPeerId(peerId),
  mStreamId(0),
  mState(IceAgentState::NeedRemoteSdp),
  mStartTime(0),
  mConnectedTime(0)
{
  mAgent = nice_agent_new(g_main_loop_get_context (mainloop),
                          NICE_COMPATIBILITY_RFC5245);
  if (mAgent == NULL)
  {
    throw std::runtime_error("nice_agent_new() failed");
  }

  g_object_set(mAgent, "stun-server", stunIp.c_str(), nullptr);
  g_object_set(mAgent, "stun-server-port", 3478, nullptr);
  g_object_set(mAgent, "controlling-mode", mOffering, nullptr);
  g_object_set(mAgent, "upnp", options.useUpnp, nullptr);

  mStreamId = nice_agent_add_stream(mAgent, 1);
  if (mStreamId == 0)
  {
    throw std::runtime_error("nice_agent_add_stream() failed");
  }


  /*
  nice_agent_set_relay_info(mAgent,
                            mStreamId,
                            1,
                            turnIp.c_str(),
                            3478,
                            mTurnUser.c_str(),
                            mTurnPassword.c_str(),
                            NICE_RELAY_TYPE_TURN_UDP);
 */
  /*
  nice_agent_set_relay_info(mAgent,
                            mStreamId,
                            1,
                            turnIp.c_str(),
                            3478,
                            mTurnUser.c_str(),
                            mTurnPassword.c_str(),
                            NICE_RELAY_TYPE_TURN_TCP);
   */

  Glib::signal_timeout().connect([this, turnIp]()
  {
    if (!isConnected())
    {
      FAF_LOG_INFO << mPeerId << ": Adding TURN server to ICE";
      nice_agent_set_relay_info(mAgent,
                                mStreamId,
                                1,
                                turnIp.c_str(),
                                3478,
                                mTurnUser.c_str(),
                                mTurnPassword.c_str(),
                                NICE_RELAY_TYPE_TURN_UDP);
    }
    return false;
  }, 1500);

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
  FAF_LOG_TRACE << "IceAgent() offer " << mOffering;
}

IceAgent::~IceAgent()
{
  if (mStreamId)
  {
    nice_agent_remove_stream(mAgent,
                             mStreamId);
  }
  if (mAgent)
  {
    g_object_unref(mAgent);
  }
  FAF_LOG_TRACE << mPeerId << ": ~IceAgent()";
}

void IceAgent::gatherCandidates()
{
  mStartTime = g_get_monotonic_time();
  // Start gathering local candidates
  if (!nice_agent_gather_candidates(mAgent,
                                    mStreamId))
  {
    throw std::runtime_error("nice_agent_gather_candidates() failed");
  }

  auto sdp = nice_agent_generate_local_sdp(mAgent);
  if (mSdpMessageCallback)
  {
    mSdpMessageCallback(this, "initialSdp", std::string(sdp));
  }
  g_free(sdp);
  g_signal_connect(mAgent,
                   "new-candidate-full",
                   G_CALLBACK(cb_new_candidate),
                   this);
}

void IceAgent::setSdpMessageCallback(SdpMessageCallback cb)
{
  mSdpMessageCallback = cb;
}

void IceAgent::setReceiveCallback(ReceiveCallback cb)
{
  mReceiveCallback = cb;
}

void IceAgent::setStateCallback(StateCallback cb)
{
  mStateCallback = cb;
}

void IceAgent::setCandidateSelectedCallback(CandidateSelectedCallback cb)
{
  mCandidateSelectedCallback = cb;
}

void IceAgent::addRemoteSdpMessage(std::string const& type, std::string const& msg)
{
  FAF_LOG_INFO << mPeerId << ": adding sdp <" << type << ">: " << msg;
  if (type == "initialSdp")
  {
    if (mHasRemoteSdp)
    {
      FAF_LOG_WARN << "already having initial SDP";
    }
    mHasRemoteSdp = true;
    if (!mRemoteSdp.empty())
    {
      FAF_LOG_WARN << "already having SDP: " << mRemoteSdp;
    }
    int res = nice_agent_parse_remote_sdp(mAgent, msg.c_str());
    if (res <= 0)
    {
      FAF_LOG_ERROR << "res " << res;
      throw std::runtime_error("nice_agent_parse_remote_sdp() failed");
    }
    if (!mOffering)
    {
      gatherCandidates();
    }
  }
  else if (type == "newCandidate")
  {
    if (mRemoteSdp.empty())
    {
      FAF_LOG_WARN << "newCandidate without prior SDP";
    }
    if (mState == IceAgentState::Ready)
    {
      return;
    }
    GSList *remote_candidates = NULL;
    NiceCandidate* c = nice_agent_parse_remote_candidate_sdp(mAgent,
                                                    mStreamId,
                                                    msg.c_str());
    if (c == NULL)
    {
      throw std::runtime_error("nice_agent_parse_remote_candidate_sdp() failed");
    }
    remote_candidates = g_slist_prepend(remote_candidates, c);
    int numCandidatesAdded = nice_agent_set_remote_candidates(mAgent,
                                                              mStreamId,
                                                              1,
                                                              remote_candidates);
    if (numCandidatesAdded <= 0)
    {
      FAF_LOG_WARN << "numCandidatesAdded " << numCandidatesAdded;
    }
    g_slist_free_full(remote_candidates, (GDestroyNotify)&nice_candidate_free);
  }
  mRemoteSdp += msg;
  mRemoteSdp += "\n";
}

bool IceAgent::hasRemoteSdp() const
{
  return mHasRemoteSdp;
}

bool IceAgent::isConnected() const
{
  return !mLocalCandidateInfo.empty();
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

std::string IceAgent::remoteSdp() const
{
  return mRemoteSdp;
}

IceAgentState IceAgent::state() const
{
  return mState;
}

double IceAgent::timeToConnected() const
{
  if (isConnected())
  {
    return (mConnectedTime - mStartTime) / 1e6;
  }
  else
  {
    return (g_get_monotonic_time() - mStartTime) / 1e6;
  }
}

void IceAgent::onCandidateGatheringDone()
{
  FAF_LOG_INFO << mPeerId << ": gathering done";
  /*
  mSdp = nice_agent_generate_local_sdp(mAgent);
  mSdp64 = g_base64_encode(reinterpret_cast<unsigned char *>(mSdp),
                           strlen(mSdp));
  if (mSdpMessageCallback)
  {
    mSdpMessageCallback(this, mSdp64);
  }
  */
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
    {
      FAF_LOG_INFO << mPeerId << ": NICE_COMPONENT_STATE_READY";
      mState = IceAgentState::Ready;
      mConnected = true;

      //auto list = nice_agent_get_local_candidates(mAgent,
      //                                            mStreamId,
      //                                            1);
      //g_slist_free_full(list, (GDestroyNotify)&nice_candidate_free);
    }
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
  return std::string(result) + ":" + std::to_string(nice_address_get_port(a));
}

void IceAgent::onCandidateSelected(NiceCandidate* localCandidate,
                                    NiceCandidate* remoteCandidate)
{
  mConnectedTime = g_get_monotonic_time();
  mLocalCandidateInfo = transportToString(localCandidate->transport);
  mLocalCandidateInfo += " ";
  mLocalCandidateInfo += addrString(&localCandidate->addr);
  mRemoteCandidateInfo = transportToString(remoteCandidate->transport);
  mRemoteCandidateInfo += " ";
  mRemoteCandidateInfo += addrString(&remoteCandidate->addr);
  if (mCandidateSelectedCallback)
  {
    mCandidateSelectedCallback(this, mLocalCandidateInfo, mRemoteCandidateInfo);
  }
}

void IceAgent::onNewCandidate(NiceCandidate* localCandidate)
{
  auto candString = nice_agent_generate_local_candidate_sdp(mAgent, localCandidate);
  if (mSdpMessageCallback)
  {
    mSdpMessageCallback(this, "newCandidate", std::string(candString));
  }
  g_free(candString);
}

void IceAgent::onReceive(std::string const& msg)
{
  if (mReceiveCallback)
  {
    if (!isConnected())
    {
      FAF_LOG_WARN << mPeerId << ": skipping " << msg.size() << " bytes: ICE not connected";
    }
    else
    {
      mReceiveCallback(this,
                       msg);
    }
  }
}

}
