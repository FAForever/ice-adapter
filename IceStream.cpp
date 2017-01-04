#include "IceStream.h"

#include <glibmm.h>

#include <agent.h>
#include <nice.h>

#include "IceAgent.h"
#include "logging.h"

#if SIGCXX_MAJOR_VERSION == 2 && SIGCXX_MICRO_VERSION == 0 && SIGCXX_MINOR_VERSION == 4
namespace sigc {
  SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}
#endif

namespace faf {

enum {
  COMPONENT_ID = 1
};

std::string stateToString(IceStreamState const& s)
{
  switch(s)
  {
    case IceStreamState::NeedRemoteSdp:
      return "NeedRemoteSdp";
      break;
    case IceStreamState::Disconnected:
      return "Disconnected";
      break;
    case IceStreamState::Gathering:
      return "Gathering";
      break;
    case IceStreamState::Connecting:
      return "Connecting";
      break;
    case IceStreamState::Connected:
      return "Connected";
      break;
    case IceStreamState::Ready:
      return "Ready";
      break;
    case IceStreamState::Failed:
      return "Failed";
      break;
  }
}

IceStream::IceStream(IceAgentPtr agent,
                     int peerId,
                     SdpCallback sdpCallback,
                     ReceiveCallback receiveCallback,
                     StateCallback stateCallback,
                     CandidateSelectedCallback candidateSelectedCallback,
                     ConnectivityChangedCallback connectivityChangedCallback,
                     IceAdapterOptions const& options):
  mAgent(agent),
  mPeerId(peerId),
  mSdpCallback(sdpCallback),
  mReceiveCallback(receiveCallback),
  mStateCallback(stateCallback),
  mCandidateSelectedCallback(candidateSelectedCallback),
  mConnectivityChangedCallback(connectivityChangedCallback),
  mStreamId(0),
  mState(IceStreamState::NeedRemoteSdp),
  mStartTime(0),
  mConnectedTime(0),
  mPingPeer(false),
  mConnectedToPeer(false),
  mPeerConnectedToMe(false),
  mLocalCandidatesGathered(false),
  mCanSend(false),
  mReadThread(nullptr),
  mReadCancellable(Gio::Cancellable::create())
{
  mStreamId = nice_agent_add_stream(mAgent->handle(), 1);
  if (mStreamId == 0)
  {
    throw std::runtime_error("nice_agent_add_stream() failed");
  }

  mTurnTimerConnection = Glib::signal_timeout().connect([this,
                                                        options]()
  {
    if (!mPeerConnectedToMe)
    {
      FAF_LOG_INFO << mPeerId << ": Adding TURN server to ICE";
      nice_agent_set_relay_info(mAgent->handle(),
                                mStreamId,
                                COMPONENT_ID,
                                options.turnIp.c_str(),
                                3478,
                                options.turnUser.c_str(),
                                options.turnPass.c_str(),
                                NICE_RELAY_TYPE_TURN_UDP);
      /* see nice_agent_forget_relays () */
    }
    return false;
  }, 2000);

  mTimerConnection = Glib::signal_timeout().connect(sigc::mem_fun(this, &IceStream::pingPeer), 500);

  if (options.iceLocalPortMin > 0)
  {
    int maxPort = options.iceLocalPortMin + 20;
    if (options.iceLocalPortMax > options.iceLocalPortMin)
    {
      maxPort = options.iceLocalPortMax;
    }
    nice_agent_set_port_range(mAgent->handle(),
                              mStreamId,
                              COMPONENT_ID,
                              static_cast<guint>(options.iceLocalPortMin),
                              static_cast<guint>(maxPort));
  }

  nice_agent_set_stream_name(mAgent->handle(),
                             mStreamId,
                             "application");

  mReadThread = new std::thread([this]()
  {
    doReadFromDifferentThread();
  });

  mReadSignaller.connect(sigc::mem_fun(this, &IceStream::onReadReady));

  FAF_LOG_DEBUG << mPeerId << ": IceStream()";
}

IceStream::~IceStream()
{
  mTimerConnection.disconnect();
  mTurnTimerConnection.disconnect();
  mReadCancellable->cancel();
  if (mReadThread->joinable())
  {
     mReadThread->join();
  }
  delete mReadThread;
  mReadThread = nullptr;
  if (mStreamId)
  {
    nice_agent_remove_stream(mAgent->handle(),
                             mStreamId);
  }
  FAF_LOG_DEBUG << mPeerId << ": ~IceStream()";
}

unsigned int IceStream::streamId() const
{
  return mStreamId;
}

int IceStream::peerId() const
{
  return mPeerId;
}

void IceStream::gatherCandidates()
{
  mStartTime = g_get_monotonic_time();
  // Start gathering local candidates
  if (!nice_agent_gather_candidates(mAgent->handle(),
                                    mStreamId))
  {
    throw std::runtime_error("nice_agent_gather_candidates() failed");
  }

  auto sdp = nice_agent_generate_local_stream_sdp(mAgent->handle(),
                                                  mStreamId,
                                                  false);
  if (mSdpCallback)
  {
    mSdpCallback(this, std::string(sdp));
  }
  g_free(sdp);
  mLocalCandidatesGathered = true;
}

void IceStream::addRemoteSdp(std::string const& sdp)
{
  FAF_LOG_INFO << mPeerId << ": adding sdp " << sdp;

  GSList *remote_candidates = nullptr;
  if (mRemoteSdp.empty())
  {
    gchar *ufrag;
    gchar *pwd;
    remote_candidates = nice_agent_parse_remote_stream_sdp(mAgent->handle(),
                                                           mStreamId,
                                                           sdp.c_str(),
                                                           &ufrag,
                                                           &pwd);
    if (remote_candidates == nullptr)
    {
      FAF_LOG_ERROR << mPeerId << ":nice_agent_parse_remote_stream_sdp() failed for " << sdp;
      return;
    }
    if (!nice_agent_set_remote_credentials(mAgent->handle(),
                                           mStreamId,
                                           ufrag,
                                           pwd))
    {
      FAF_LOG_ERROR << mPeerId << ":nice_agent_set_remote_credentials() failed for " <<
                       sdp <<
                       " " << ufrag <<
                       " " << pwd;
      return;
    }
    g_free(ufrag);
    g_free(pwd);
  }
  else
  {
    if (mState == IceStreamState::Ready)
    {
      FAF_LOG_DEBUG << mPeerId << ": connected - dropping remote SDP " << sdp;
      return;
    }
    NiceCandidate* c = nice_agent_parse_remote_candidate_sdp(mAgent->handle(),
                                                             mStreamId,
                                                             sdp.c_str());
    if (c == nullptr)
    {
      FAF_LOG_ERROR << mPeerId << ": nice_agent_parse_remote_candidate_sdp() failed for " << sdp;
      return;
     }
    remote_candidates = g_slist_append(remote_candidates, c);
  }
  if (remote_candidates == nullptr)
  {
    FAF_LOG_ERROR << mPeerId << ":addRemoteSdp() didn't parse SDP from " << sdp;
    return;
  }
  int numCandidatesAdded = nice_agent_set_remote_candidates(mAgent->handle(),
                                                            mStreamId,
                                                            COMPONENT_ID,
                                                            remote_candidates);
  if (numCandidatesAdded <= 0)
  {
    FAF_LOG_WARN << "numCandidatesAdded " << numCandidatesAdded;
  }
  g_slist_free_full(remote_candidates,
                    reinterpret_cast<GDestroyNotify>(&nice_candidate_free));
  mRemoteSdp += sdp;
}

void IceStream::send(std::string const& msg)
{
  if (!mCanSend)
  {
    FAF_LOG_WARN << "!mCanSend: " << msg;
    return;
  }
  nice_agent_send(mAgent->handle(),
                  mStreamId,
                  COMPONENT_ID,
                  msg.size(),
                  msg.c_str());
#if 0
  GOutputVector v = {msg.c_str(), msg.size()};
  NiceOutputMessage m = { &v, 1};
  GError* error = nullptr;
  if (!mCanSend)
  {
    FAF_LOG_WARN << "!mCanSend: " << msg;
    //return;
  }
  nice_agent_send_messages_nonblocking (mAgent->handle(),
                                        mStreamId,
                                        COMPONENT_ID,
                                        &m,
                                        1,
                                        nullptr,
                                        &error);
  if (error)
  {
    FAF_LOG_ERROR << "error in nice_agent_send_messages_nonblocking(): " << error->message;
    g_clear_error(&error);
  }
#endif
}

std::string IceStream::localCandidateInfo() const
{
  return mLocalCandidateInfo;
}

std::string IceStream::remoteCandidateInfo() const
{
  return mRemoteCandidateInfo;
}

std::string IceStream::remoteSdp() const
{
  return mRemoteSdp;
}

IceStreamState IceStream::state() const
{
  return mState;
}

double IceStream::timeToConnected() const
{
  if (mStartTime > 0 && mConnectedTime > 0)
  {
    return (mConnectedTime - mStartTime) / 1e6;
  }
  return 0;
}

bool IceStream::peerConnectedToMe() const
{
  return mPeerConnectedToMe;
}

bool IceStream::connectedToPeer() const
{
  return mConnectedToPeer;
}

void IceStream::setConnectivity(bool connectedToPeer, bool peerConnectedToMe)
{
  if (peerConnectedToMe &&
      mConnectedTime == 0)
  {
    mConnectedTime = g_get_monotonic_time();
  }
  if (mConnectedToPeer != connectedToPeer ||
      mPeerConnectedToMe != peerConnectedToMe)
  {
    if (mConnectivityChangedCallback)
    {
      mConnectivityChangedCallback(this,
                                   peerConnectedToMe,
                                   connectedToPeer);
    }
    mPeerConnectedToMe = peerConnectedToMe;
    mConnectedToPeer = connectedToPeer;
  }
}

void IceStream::onCandidateGatheringDone()
{
  FAF_LOG_INFO << mPeerId << ": gathering done";
}

void IceStream::onComponentStateChanged(unsigned int state)
{
  switch (state)
  {
    case NICE_COMPONENT_STATE_DISCONNECTED:
      FAF_LOG_WARN << "NICE_COMPONENT_STATE_DISCONNECTED";
      mState = IceStreamState::Disconnected;
      break;
    case NICE_COMPONENT_STATE_GATHERING:
      FAF_LOG_TRACE << "NICE_COMPONENT_STATE_GATHERING";
      mState = IceStreamState::Gathering;
      break;
    case NICE_COMPONENT_STATE_CONNECTING:
      FAF_LOG_TRACE << "NICE_COMPONENT_STATE_CONNECTING";
      mState = IceStreamState::Connecting;
      break;
    case NICE_COMPONENT_STATE_CONNECTED:
      FAF_LOG_TRACE << "NICE_COMPONENT_STATE_CONNECTED";
      mState = IceStreamState::Connected;
      break;
    case NICE_COMPONENT_STATE_READY:
    {
      FAF_LOG_INFO << mPeerId << ": NICE_COMPONENT_STATE_READY";
      mState = IceStreamState::Ready;
      mCanSend = true;
      //setConnectivity(mConnectedToPeer, true);
    }
      break;
    case NICE_COMPONENT_STATE_FAILED:
      FAF_LOG_ERROR << "NICE_COMPONENT_STATE_FAILED";
      mState = IceStreamState::Failed;
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

void IceStream::onCandidateSelected(NiceCandidate* localCandidate,
                         NiceCandidate* remoteCandidate)
{
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
  if (!mConnectedToPeer)
  {
    setConnectivity(true, mPeerConnectedToMe);
    mPingPeer = true;
    pingPeer();
  }
}

void IceStream::onNewCandidate(NiceCandidate* localCandidate)
{
  if (mLocalCandidatesGathered)
  {
    auto candString = nice_agent_generate_local_candidate_sdp(mAgent->handle(),
                                                              localCandidate);
    if (mSdpCallback)
    {
      mSdpCallback(this, std::string(candString));
    }
    g_free(candString);
  }
}

void IceStream::onReadReady()
{
  std::lock_guard<std::mutex> lock(mReadMutex);
  while (!mReadQueue.empty())
  {
    if (mReadQueue.front() == "FAFPING")
    {
      send("FAFPONG");
    }
    else if (mReadQueue.front() == "FAFPONG")
    {
      setConnectivity(mConnectedToPeer, true);
    }
    else if (mReceiveCallback)
    {
      mReceiveCallback(this,
                       mReadQueue.front());
    }
    mReadQueue.pop();
  }
}

void IceStream::doReadFromDifferentThread()
{
  char buf[2048];
  GInputVector vec = {buf, 2048};
  NiceInputMessage msg = {&vec, 1, NULL, 0 };
  GError* error = nullptr;

  while (!mReadCancellable->is_cancelled())
  {
    int messageCount = nice_agent_recv_messages(mAgent->handle(),
                                                mStreamId,
                                                COMPONENT_ID,
                                                &msg,
                                                1,
                                                mReadCancellable->gobj(),
                                                &error);
    if (messageCount > 0)
    {
      {
        std::lock_guard<std::mutex> lock(mReadMutex);
        for (int iMsg = 0; iMsg < messageCount; ++iMsg)
        {
          mReadQueue.push(std::string(buf, msg.length));
        }
      }
      mReadSignaller.emit();
    }
    if (error)
    {
      FAF_LOG_ERROR << "error in nice_agent_recv_messages(): " << error->message;
      g_clear_error(&error);
    }
  }
  FAF_LOG_INFO << "doReadFromDifferentThread() finished";
}

bool IceStream::pingPeer()
{
  if (mPingPeer)
  {
    send("FAFPING");
  }
  return true;
}

} // namespace faf
