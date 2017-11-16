#include "Timer.h"

#include <webrtc/rtc_base/bind.h>
#include <webrtc/rtc_base/thread.h>

#include "logging.h"

namespace faf {

Timer::Timer():
  _interval(0)
{
}

void Timer::start(int intervalMs, std::function<void()> callback)
{
  if (!_callback && intervalMs > 0)
  {
    _interval = intervalMs;
    _callback = callback;
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, _interval, this);
  }
}

void Timer::stop()
{
  _callback = std::function<void()>();
}

void Timer::OnMessage(rtc::Message* msg)
{
  if (_callback)
  {
    _callback();
    rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, _interval, this);
  }
}

} // namespace faf
