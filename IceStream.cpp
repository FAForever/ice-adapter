#include "IceStream.h"

#include <iostream>

#include <cstring>

#include <nice.h>

#include "IceAgent.h"

void
cb_nice_recv(NiceAgent *agent,
             guint stream_id,
             guint component_id,
             guint len,
             gchar *buf,
             gpointer data)
{
  static_cast<IceStream*>(data)->onReceive(std::string(buf, len));
}

IceStream::IceStream(unsigned int streamId,
                     IceAgent* agent,
                     GMainLoop* mainloop):
  mStreamId(streamId),
  mAgent(agent),
  mSdp(nullptr),
  mSdp64(nullptr),
  mHasRemoteSdp(false),
  mConnected(false)
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

void IceStream::setReceiveCallback(ReceiveCallback cb)
{
  mReceiveCallback = cb;
}


void IceStream::setRemoteSdp(std::string const& sdpBase64)
{
  gsize sdp_len;
  char* sdp = reinterpret_cast<char*>(g_base64_decode (sdpBase64.c_str(), &sdp_len));

  int res = nice_agent_parse_remote_sdp(mAgent->agent(), sdp);
  g_free (sdp);
  if (res <= 0)
  {
    std::cerr << "res " << res << std::endl;
    throw std::runtime_error("nice_agent_parse_remote_sdp() failed");
  }
  mHasRemoteSdp = true;
}

bool IceStream::hasRemoteSdp() const
{
  return mHasRemoteSdp;
}

bool IceStream::isConnected() const
{
  return mConnected;
}

void IceStream::send(std::string const& msg)
{
  if (isConnected())
  {
    nice_agent_send(mAgent->agent(),
                    mStreamId,
                    1,
                    msg.size(),
                    msg.c_str());
  }
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
  //std::cout << "SDP:\n" << mSdp << std::endl;
}

void IceStream::onComponentStateChanged(unsigned int componentId,
                                        unsigned int state)
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
      mConnected = true;
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

void IceStream::onCandidateSelected(NiceCandidate* localCandidate,
                                    NiceCandidate* remoteCandidate)
{

  std::cout << "selected Candidate: \n\t"
            << "\tlocal:" << transportToString(localCandidate->transport) << " " << addrString(&localCandidate->addr) << "\n"
            << "\tremote:" << transportToString(remoteCandidate->transport) << " " << addrString(&remoteCandidate->addr) << std::endl;
}

void IceStream::onReceive(std::string const& msg)
{
  if (mReceiveCallback)
  {
    mReceiveCallback(this,
                     msg);
  }
}
