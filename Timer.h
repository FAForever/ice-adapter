#pragma once

#include <functional>

#include <webrtc/rtc_base/messagehandler.h>

namespace faf {

class Timer : public rtc::MessageHandler
{
public:
  Timer();
  virtual ~Timer();

  void start(int intervalMs, std::function<void()> callback);
  void singleShot(int delay, std::function<void()> callback);
  bool started() const;
  void stop();
protected:
  virtual void OnMessage(rtc::Message* msg) override;
  int _interval;
  std::function<void()> _callback;
  bool _singleShot{false};

  RTC_DISALLOW_COPY_AND_ASSIGN(Timer);
};

} // namespace faf
