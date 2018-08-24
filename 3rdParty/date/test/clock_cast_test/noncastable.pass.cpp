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
#include <type_traits>
#include <cassert>

template<typename SourceClock, typename DestClock, typename = void>
struct is_clock_castable
  : std::false_type
{};

template<typename SourceClock, typename DestClock>
struct is_clock_castable<SourceClock, DestClock, decltype(date::clock_cast<DestClock>(typename SourceClock::time_point()), void())>
  : std::true_type
{};


//Clock based on steady clock, not related to wall time (sys_clock/utc_clock)
struct steady_based_clock
{
  using duration = std::chrono::steady_clock::duration;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<steady_based_clock, duration>;

  static time_point now()
  {
    return time_point(std::chrono::steady_clock::now().time_since_epoch());
  }
};

//Traits that allow conversion between steady_clock and steady_based clock
//Does not use wall-time clocks as rally (sys/utc)
namespace date
{
   template<>
   struct clock_time_conversion<std::chrono::steady_clock, steady_based_clock>
   {
     template<typename Duration>
     std::chrono::time_point<std::chrono::steady_clock, Duration>
     operator()(std::chrono::time_point<steady_based_clock, Duration> const& tp) const
     {
       using res = std::chrono::time_point<std::chrono::steady_clock, Duration>;
       return res(tp.time_since_epoch());
     }
   };

   template<>
   struct clock_time_conversion<steady_based_clock, std::chrono::steady_clock>
   {
     template<typename Duration>
     std::chrono::time_point<steady_based_clock, Duration>
     operator()(std::chrono::time_point<std::chrono::steady_clock, Duration> const& tp) const
     {
       using res = std::chrono::time_point<steady_based_clock, Duration>;
       return res(tp.time_since_epoch());
     }
   };
}

//Ambigous clocks both providing to/from_sys and to/from_utc
//They are mock_ups just returning zero time_point
struct amb1_clock
{
   using duration = std::chrono::seconds;
   using rep = duration::rep;
   using period = duration::period;
   using time_point = std::chrono::time_point<amb1_clock>;

   static time_point now()
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<std::chrono::system_clock, Duration>
   to_sys(std::chrono::time_point<amb1_clock, Duration> const&)
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<amb1_clock, Duration>
   from_sys(std::chrono::time_point<std::chrono::system_clock, Duration> const&)
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<date::utc_clock, Duration>
   to_utc(std::chrono::time_point<amb1_clock, Duration> const&)
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<amb1_clock, Duration>
   from_utc(std::chrono::time_point<date::utc_clock, Duration> const&)
   {
     return {};
   }
};

struct amb2_clock
{
   using duration = std::chrono::seconds;
   using rep = duration::rep;
   using period = duration::period;
   using time_point = std::chrono::time_point<amb2_clock>;

   static time_point now()
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<std::chrono::system_clock, Duration>
   to_sys(std::chrono::time_point<amb2_clock, Duration> const&)
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<amb2_clock, Duration>
   from_sys(std::chrono::time_point<std::chrono::system_clock, Duration> const&)
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<date::utc_clock, Duration>
   to_utc(std::chrono::time_point<amb2_clock, Duration> const&)
   {
     return {};
   }

   template<typename Duration>
   static
   std::chrono::time_point<amb2_clock, Duration>
   from_utc(std::chrono::time_point<date::utc_clock, Duration> const&)
   {
     return {};
   }
};

namespace date
{
   //Disambiguates that sys_clock is preffered
   template<>
   struct clock_time_conversion<amb1_clock, amb2_clock>
   {
     template<typename Duration>
     std::chrono::time_point<amb1_clock, Duration>
     operator()(std::chrono::time_point<amb2_clock, Duration> const& tp) const
     {
       return amb1_clock::from_sys(amb2_clock::to_sys(tp));
     }
   };
}

int
main()
{
    using namespace date;
    using namespace std::chrono;
    using sys_clock = std::chrono::system_clock;

    //steady_clock (must be different that sys_clock)
    static_assert(is_clock_castable<steady_clock, steady_clock>::value, "steady_clock -> steady_clock");
    static_assert(!is_clock_castable<steady_clock, sys_clock>::value, "steady_clock -> sys_clock");
    static_assert(!is_clock_castable<sys_clock, steady_clock>::value, "sys_clock -> steady_clock");
    static_assert(!is_clock_castable<steady_clock, utc_clock>::value, "steady_clock -> utc_clock");
    static_assert(!is_clock_castable<utc_clock, steady_clock>::value, "utc_clock -> steady_clock");
    static_assert(!is_clock_castable<steady_clock, tai_clock>::value, "steady_clock -> tai_clock");
    static_assert(!is_clock_castable<tai_clock, steady_clock>::value, "tai_clock -> steady_clock");

    //steady_based_clock (unrelated to sys_clock and utc_clocks)
    static_assert(is_clock_castable<steady_based_clock, steady_based_clock>::value, "steady_based_clock -> steady_based_clock");
    static_assert(!is_clock_castable<steady_based_clock, sys_clock>::value, "steady_based_clock -> sys_clock");
    static_assert(!is_clock_castable<sys_clock, steady_based_clock>::value, "sys_clock -> steady_based_clock");
    static_assert(!is_clock_castable<steady_based_clock, utc_clock>::value, "steady_based_clock -> utc_clock");
    static_assert(!is_clock_castable<utc_clock, steady_based_clock>::value, "utc_clock -> steady_based_clock");
    static_assert(!is_clock_castable<steady_based_clock, tai_clock>::value, "steady_based_clock -> tai_clock");
    static_assert(!is_clock_castable<tai_clock, steady_based_clock>::value, "tai_clock -> steady_based_clock");

    //steady_based <-> steady_clock
    {
      auto s1 = steady_clock::time_point(steady_clock::duration(200));
      auto s2 = steady_based_clock::time_point(steady_based_clock::duration(200));

      assert(clock_cast<steady_based_clock>(s1) == s2);
      assert(clock_cast<steady_clock>(s2) == s1);
    }

    //ambX <-> sys/utc works as one rally can be used in each case, or one lead to quicker conversione
    static_assert(is_clock_castable<amb1_clock, amb1_clock>::value, "amb1_clock -> amb1_clock");
    static_assert(is_clock_castable<amb1_clock, sys_clock>::value, "amb1_clock -> sys_clock");
    static_assert(is_clock_castable<sys_clock, amb1_clock>::value, "sys_clock -> amb1_clock");
    static_assert(is_clock_castable<amb1_clock, utc_clock>::value, "amb1_clock -> utc_clock");
    static_assert(is_clock_castable<utc_clock, amb1_clock>::value, "utc_clock -> amb1_clock");
    static_assert(is_clock_castable<amb1_clock, tai_clock>::value, "amb1_clock -> tai_clock");
    static_assert(is_clock_castable<tai_clock, amb1_clock>::value, "tai_clock -> amb1_clock");
    static_assert(is_clock_castable<amb1_clock, tai_clock>::value, "amb1_clock -> tai_clock");
    static_assert(is_clock_castable<gps_clock, amb1_clock>::value, "gps_clock -> amb1_clock");
    static_assert(is_clock_castable<amb2_clock, amb2_clock>::value, "amb2_clock -> amb2_clock");
    static_assert(is_clock_castable<amb2_clock, sys_clock>::value, "amb2_clock -> sys_clock");
    static_assert(is_clock_castable<sys_clock, amb2_clock>::value, "sys_clock -> amb2_clock");
    static_assert(is_clock_castable<amb2_clock, utc_clock>::value, "amb2_clock -> utc_clock");
    static_assert(is_clock_castable<utc_clock, amb2_clock>::value, "utc_clock -> amb2_clock");
    static_assert(is_clock_castable<amb2_clock, tai_clock>::value, "amb2_clock -> tai_clock");
    static_assert(is_clock_castable<tai_clock, amb2_clock>::value, "tai_clock -> amb2_clock");
    static_assert(is_clock_castable<amb2_clock, tai_clock>::value, "amb2_clock -> tai_clock");
    static_assert(is_clock_castable<gps_clock, amb2_clock>::value, "gps_clock -> amb2_clock");

    //amb1 -> amb2: ambigous because can either go trough sys_clock or utc_clock
    static_assert(!is_clock_castable<amb1_clock, amb2_clock>::value, "amb1_clock -> amb2_clock");

    //amb2 -> amb1: disambiguated via trait specialization
    static_assert(is_clock_castable<amb2_clock, amb1_clock>::value, "amb2_clock -> amb1_clock");
}
