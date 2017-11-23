#pragma once

#include <functional>

#include <webrtc/rtc_base/sigslot.h>
#include <webrtc/rtc_base/messagehandler.h>

namespace faf {

class Timer : public rtc::MessageHandler
{
public:
  Timer();

  void start(int intervalMs, std::function<void()> callback);
  void stop();
protected:
  virtual void OnMessage(rtc::Message* msg) override;
  int _interval;
  std::function<void()> _callback;

  RTC_DISALLOW_COPY_AND_ASSIGN(Timer);
};

} // namespace faf
