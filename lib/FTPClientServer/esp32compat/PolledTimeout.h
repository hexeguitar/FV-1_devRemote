#ifndef __POLLEDTIMING_H__
#define __POLLEDTIMING_H__


/*
 PolledTimeout.h - Encapsulation of a polled Timeout
 
 Copyright (c) 2018 Daniel Salazar. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <limits>

#include <Arduino.h>

namespace esp32
{


namespace polledTimeout
{

namespace YieldPolicy
{

struct DoNothing
{
  static void execute() {}
};

struct YieldOrSkip
{
  static void execute() {delay(0);}
};

template <unsigned long delayMs>
struct YieldAndDelayMs
{
  static void execute() {delay(delayMs);}
};

} //YieldPolicy

namespace TimePolicy
{

struct TimeSourceMillis
{
  // time policy in milli-seconds based on millis()

  using timeType = decltype(millis());
  static timeType time() {return millis();}
  static constexpr timeType ticksPerSecond    = 1000;
  static constexpr timeType ticksPerSecondMax = 1000;
};

template <typename TimeSourceType, unsigned long long second_th>
    // "second_th" units of timeType for one second
struct TimeUnit
{
  using timeType = typename TimeSourceType::timeType;

#if __GNUC__ < 5
  // gcc-4.8 cannot compile the constexpr-only version of this function
  // using #defines instead luckily works
  static constexpr timeType computeRangeCompensation ()
  {
    #define number_of_secondTh_in_one_tick ((1.0 * second_th) / ticksPerSecond)
    #define fractional (number_of_secondTh_in_one_tick - (long)number_of_secondTh_in_one_tick)

    return ({
      fractional == 0?
        1: // no need for compensation
        (number_of_secondTh_in_one_tick / fractional) + 0.5; // scalar multiplier allowing exact division
    });

    #undef number_of_secondTh_in_one_tick
    #undef fractional
  }
#else
  static constexpr timeType computeRangeCompensation ()
  {
    return ({
      constexpr double number_of_secondTh_in_one_tick = (1.0 * second_th) / ticksPerSecond;
      constexpr double fractional = number_of_secondTh_in_one_tick - (long)number_of_secondTh_in_one_tick;
      fractional == 0?
        1: // no need for compensation
        (number_of_secondTh_in_one_tick / fractional) + 0.5; // scalar multiplier allowing exact division
    });
  }
#endif

  static constexpr timeType ticksPerSecond         = TimeSourceType::ticksPerSecond;
  static constexpr timeType ticksPerSecondMax      = TimeSourceType::ticksPerSecondMax;
  static constexpr timeType rangeCompensate        = computeRangeCompensation();
  static constexpr timeType user2UnitMultiplierMax = (ticksPerSecondMax * rangeCompensate) / second_th;
  static constexpr timeType user2UnitMultiplier    = (ticksPerSecond    * rangeCompensate) / second_th;
  static constexpr timeType user2UnitDivider       = rangeCompensate;
  // std::numeric_limits<timeType>::max() is reserved
  static constexpr timeType timeMax                = (std::numeric_limits<timeType>::max() - 1) / user2UnitMultiplierMax;

  static timeType toTimeTypeUnit (const timeType userUnit) {return (userUnit * user2UnitMultiplier) / user2UnitDivider;}
  static timeType toUserUnit (const timeType internalUnit) {return (internalUnit * user2UnitDivider) / user2UnitMultiplier;}
  static timeType time () {return TimeSourceType::time();}
};

using TimeMillis     = TimeUnit< TimeSourceMillis,       1000 >;

} //TimePolicy

template <bool PeriodicT, typename YieldPolicyT = YieldPolicy::DoNothing, typename TimePolicyT = TimePolicy::TimeMillis>
class timeoutTemplate
{
public:
  using timeType = typename TimePolicyT::timeType;
  static_assert(std::is_unsigned<timeType>::value == true, "timeType must be unsigned");

  static constexpr timeType alwaysExpired   = 0;
  static constexpr timeType neverExpires    = std::numeric_limits<timeType>::max();
  static constexpr timeType rangeCompensate = TimePolicyT::rangeCompensate; //debug

  timeoutTemplate(const timeType userTimeout)
  {
    reset(userTimeout);
  }

  IRAM_ATTR // fast
  bool expired()
  {
    YieldPolicyT::execute(); //in case of DoNothing: gets optimized away
    if(PeriodicT)           //in case of false: gets optimized away
      return expiredRetrigger();
    return expiredOneShot();
  }
  
  IRAM_ATTR // fast
  operator bool()
  {
    return expired(); 
  }
  
  bool canExpire () const
  {
    return !_neverExpires;
  }

  bool canWait () const
  {
    return _timeout != alwaysExpired;
  }

  IRAM_ATTR // called from ISR
  void reset(const timeType newUserTimeout)
  {
    reset();
    _timeout = TimePolicyT::toTimeTypeUnit(newUserTimeout);
    _neverExpires = (newUserTimeout < 0) || (newUserTimeout > timeMax());
  }

  IRAM_ATTR // called from ISR
  void reset()
  {
    _start = TimePolicyT::time();
  }

  void resetToNeverExpires ()
  {
    _timeout = alwaysExpired + 1; // because canWait() has precedence
    _neverExpires = true;
  }

  timeType getTimeout() const
  {
    return TimePolicyT::toUserUnit(_timeout);
  }
  
  static constexpr timeType timeMax()
  {
    return TimePolicyT::timeMax;
  }

private:

  IRAM_ATTR // fast
  bool checkExpired(const timeType internalUnit) const
  {
    // canWait() is not checked here
    // returns "can expire" and "time expired"
    return (!_neverExpires) && ((internalUnit - _start) >= _timeout);
  }

protected:

  IRAM_ATTR // fast
  bool expiredRetrigger()
  {
    if (!canWait())
      return true;

    timeType current = TimePolicyT::time();
    if(checkExpired(current))
    {
      unsigned long n = (current - _start) / _timeout; //how many _timeouts periods have elapsed, will usually be 1 (current - _start >= _timeout)
      _start += n  * _timeout;
      return true;
    }
    return false;
  }
  
  IRAM_ATTR // fast
  bool expiredOneShot() const
  {
    // returns "always expired" or "has expired"
    return !canWait() || checkExpired(TimePolicyT::time());
  }
  
  timeType _timeout;
  timeType _start;
  bool _neverExpires;
};

// standard versions (based on millis())
// timeMax() is 49.7 days ((2^32)-2 ms)

using oneShotMs = polledTimeout::timeoutTemplate<false>;
using periodicMs = polledTimeout::timeoutTemplate<true>;


} //polledTimeout


/* A 1-shot timeout that auto-yields when in CONT can be built as follows:
 * using oneShotYieldMs = esp32::polledTimeout::timeoutTemplate<false, esp32::polledTimeout::YieldPolicy::YieldOrSkip>;
 *
 * Other policies can be implemented by the user, e.g.: simple yield that panics in SYS, and the polledTimeout types built as needed as shown above, without modifying this file.
 */

}//esp32

#endif
