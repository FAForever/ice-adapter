#pragma once

#include "IceAdapterOptions.h"
#include "GPGNetServer.h"

#include <webrtc/base/scoped_ref_ptr.h>
#include <webrtc/api/peerconnectioninterface.h>

namespace faf {

class IceAdapter
{
public:
  IceAdapter(int argc, char *argv[]);
  virtual ~IceAdapter();

  void run();
protected:
  IceAdapterOptions _options;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _pcfactory;
  GPGNetServer _gpgnetServer;
};

} // namespace faf
