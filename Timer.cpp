#include "Timer.h"

#include <webrtc/rtc_base/bind.h>
#include <webrtc/rtc_base/thread.h>

#include "logging.h"

#ifdef WEBRTC_POSIX /* dirty fix for linker errors */
namespace rtc
{
MessageHandler::~MessageHandler()
{
  MessageQueueManager::Clear(this);
}
}
#endif

namespace faf {

Timer::Timer():
  _interval(0)
{
}

Timer::~Timer()
{
  stop();
}

void Timer::start(int intervalMs, std::function<void()> callback)
{
  stop();
  _interval = intervalMs;
  _callback = callback;
  _singleShot = false;
  rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, _interval, this);
}

void Timer::singleShot(int delay, std::function<void()> callback)
{
  stop();
  _callback = callback;
  _singleShot = true;
  rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, delay, this);
}

bool Timer::started() const
{
  return static_cast<bool>(_callback);
}

void Timer::stop()
{
  _callback = std::function<void()>();
  rtc::Thread::Current()->Clear(this);
}

void Timer::OnMessage(rtc::Message* msg)
{
  if (_callback)
  {
    _callback();
    if (!_singleShot)
    {
      /* reset _callback, to make started() return false after this callback */
      _callback = std::function<void()>();
    }
    else
    {
      rtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, _interval, this);
    }
  }
}

} // namespace faf
