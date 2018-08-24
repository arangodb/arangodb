// The MIT License (MIT)
//
// Copyright (c) 2017 Tomasz Kamiński
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "tz.h"
#include <cassert>
#include <type_traits>

//used to count number of conversion
int conversions = 0;

//to/from impl
struct mil_clock
{
  using duration = std::common_type_t<std::chrono::system_clock::duration, date::days>;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<mil_clock, duration>;

  static constexpr date::sys_days epoch = date::year{2000}/date::month{0}/date::day{1};

  template<typename Duration>
  static
  std::chrono::time_point<std::chrono::system_clock, std::common_type_t<Duration, date::days>>
  to_sys(std::chrono::time_point<mil_clock, Duration> const& tp)
  {
    ++conversions;
    return epoch + tp.time_since_epoch();
  }

  template<typename Duration>
  static
  std::chrono::time_point<mil_clock, std::common_type_t<Duration, date::days>>
  from_sys(std::chrono::time_point<std::chrono::system_clock, Duration> const& tp)
  {
    ++conversions;
    using res = std::chrono::time_point<mil_clock, std::common_type_t<Duration, date::days>>;
    return res(tp - epoch);
  }

  static time_point now()
  {
    return from_sys(std::chrono::system_clock::now());
  }
};


date::sys_days const mil_clock::epoch;

// traits example
struct s2s_clock
{
  using duration = std::chrono::system_clock::duration;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<s2s_clock, duration>;

  template<typename Duration>
  static
  std::chrono::time_point<std::chrono::system_clock, Duration>
  to_sys(std::chrono::time_point<s2s_clock, Duration> const& tp)
  {
    ++conversions;
    return std::chrono::time_point<std::chrono::system_clock, Duration>(tp.time_since_epoch());
  }

  template<typename Duration>
  static
  std::chrono::time_point<s2s_clock, Duration>
  from_sys(std::chrono::time_point<std::chrono::system_clock, Duration> const& tp)
  {
    ++conversions;
    return std::chrono::time_point<s2s_clock, Duration>(tp.time_since_epoch());
  }

  static time_point now()
  {
    return from_sys(std::chrono::system_clock::now());
  }
};

namespace date
{
   template<>
   struct clock_time_conversion<mil_clock, s2s_clock>
   {
     template<typename Duration>
     std::chrono::time_point<mil_clock, std::common_type_t<Duration, date::days>>
     operator()(std::chrono::time_point<s2s_clock, Duration> const& tp)
     {
       ++conversions;
       using res = std::chrono::time_point<mil_clock, std::common_type_t<Duration, date::days>>;
       return res(tp.time_since_epoch() - mil_clock::epoch.time_since_epoch());
     }
   };
}

int
main()
{
    using namespace date;
    using sys_clock = std::chrono::system_clock;

    // self
    {
       sys_days st(1997_y/dec/12);
       auto mt = mil_clock::from_sys(st);

       assert(clock_cast<mil_clock>(mt) == mt);
    }

    // mil <-> sys
    {
       sys_days st(1997_y/dec/12);
       auto mt = mil_clock::from_sys(st);

       assert(clock_cast<mil_clock>(st) == mt);
       assert(clock_cast<sys_clock>(mt) == st);
    }

    // mil <-> utc
    {
       sys_days st(1997_y/dec/12);
       auto mt = mil_clock::from_sys(st);
       auto ut = utc_clock::from_sys(st);

       assert(clock_cast<mil_clock>(ut) == mt);
       assert(clock_cast<utc_clock>(mt) == ut);
    }

    // mil <-> tai
    {
       sys_days st(1997_y/dec/12);
       auto mt = mil_clock::from_sys(st);
       auto ut = utc_clock::from_sys(st);
       auto tt = tai_clock::from_utc(ut);

       assert(clock_cast<tai_clock>(mt) == tt);
       assert(clock_cast<mil_clock>(tt) == mt);
    }

    // mil <-> gps
    {
       sys_days st(1997_y/dec/12);
       auto mt = mil_clock::from_sys(st);
       auto ut = utc_clock::from_sys(st);
       auto gt = gps_clock::from_utc(ut);

       assert(clock_cast<gps_clock>(mt) == gt);
       assert(clock_cast<mil_clock>(gt) == mt);
    }

    // s2s -> mil
    {
       sys_days st(1997_y/dec/12);
       auto mt = mil_clock::from_sys(st);
       auto s2t = s2s_clock::from_sys(st);

       //direct trait conversion
       conversions = 0;
       assert(clock_cast<mil_clock>(s2t) == mt);
       assert(conversions == 1);

       //uses sys_clock
       conversions = 0;
       assert(clock_cast<s2s_clock>(mt) == s2t);
       assert(conversions == 2);
    }
}
