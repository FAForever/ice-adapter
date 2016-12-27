#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <array>

#include <sigc++/sigc++.h>

typedef struct _NiceAgent NiceAgent;
typedef struct _NiceCandidate NiceCandidate;

namespace faf {

class IceAdapterOptions;
class IceAgent;
typedef std::shared_ptr<IceAgent> IceAgentPtr;

enum class IceStreamState
{
  NeedRemoteSdp,
  Disconnected,
  Gathering,
  Connecting,
  Connected,
  Ready,
  Failed
};

struct IceStreamPacket
{
  struct Header
  {
    enum Magic : uint32_t
    {
      MAGIC = 0xfaf001ce
    } magic;
    uint16_t length;
  } header;
  char data[2048];
};

std::string stateToString(IceStreamState const& s);

class IceStream
{
public:
  typedef std::function<void (IceStream*, std::string const&)> SdpCallback;
  typedef std::function<void (IceStream*, std::string const&)> ReceiveCallback;
  typedef std::function<void (IceStream*, IceStreamState const&)> StateCallback;
  typedef std::function<void (IceStream*, std::string const&, std::string const&)> CandidateSelectedCallback;
  typedef std::function<void (IceStream*, bool, bool)> ConnectivityChangedCallback;

  IceStream(IceAgentPtr agent,
            int peerId,
            SdpCallback sdpCallback,
            ReceiveCallback receiveCallback,
            StateCallback stateCallback,
            CandidateSelectedCallback candidateSelectedCallback,
            ConnectivityChangedCallback connectivityChangedCallback,
            IceAdapterOptions const& options);
  virtual ~IceStream();

  unsigned int streamId() const;
  int peerId() const;

  void gatherCandidates();

  void addRemoteSdp(std::string const& sdp);
  void send(std::string const& msg);

  std::string localCandidateInfo() const;
  std::string remoteCandidateInfo() const;
  std::string remoteSdp() const;

  IceStreamState state() const;

  double timeToConnected() const;

  bool peerConnectedToMe() const;
  bool connectedToPeer() const;

protected:
  void setConnectivity(bool connectedToPeer, bool peerConnectedToMe);
  void onReceive(std::string const& msg);
  void onCandidateGatheringDone();
  void onComponentStateChanged(unsigned int state);
  void onCandidateSelected(NiceCandidate* localCandidate,
                           NiceCandidate* remoteCandidate);
  void onNewCandidate(NiceCandidate* localCandidate);
  bool pingPeer();

  IceAgentPtr mAgent;
  int mPeerId;
  SdpCallback mSdpCallback;
  ReceiveCallback mReceiveCallback;
  StateCallback mStateCallback;
  CandidateSelectedCallback mCandidateSelectedCallback;
  ConnectivityChangedCallback mConnectivityChangedCallback;

  unsigned int mStreamId;
  IceStreamState mState;
  int64_t mStartTime;
  int64_t mConnectedTime;
  bool mPingPeer;
  bool mConnectedToPeer;
  bool mPeerConnectedToMe;
  bool mLocalCandidatesGathered;
  std::string mLocalCandidateInfo;
  std::string mRemoteCandidateInfo;
  std::string mRemoteSdp;
  IceStreamPacket mCurrentPacket;

  sigc::connection mTimerConnection;
  sigc::connection mTurnTimerConnection;

  friend void cb_candidate_gathering_done(NiceAgent*,
                                          unsigned int,
                                          void*);
  friend void cb_component_state_changed(NiceAgent *agent,
                                         unsigned int,
                                         unsigned int,
                                         unsigned int,
                                         void*);
  friend void cb_new_selected_pair_full(NiceAgent*,
                                        unsigned int,
                                        unsigned int,
                                        NiceCandidate*,
                                        NiceCandidate*,
                                        void*);
  friend void cb_new_candidate(NiceAgent*,
                               NiceCandidate*,
                               void*);
  friend void cb_nice_recv(NiceAgent*,
                           unsigned int,
                           unsigned int,
                           unsigned int,
                           char*,
                           void*);
};

typedef std::shared_ptr<IceStream> IceStreamPtr;

} // namespace faf
