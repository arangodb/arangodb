//  Copyright 2010 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_CHRONO_TEST_CYCLE_COUNT_HPP
#define BOOST_CHRONO_TEST_CYCLE_COUNT_HPP

#include <boost/chrono/ceil.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/chrono/time_point.hpp>
#include <boost/chrono/stopwatches/reporters/stopwatch_reporter_default_formatter.hpp>
#include <boost/chrono/stopwatches/formatters/elapsed_formatter.hpp>
#include <boost/chrono/stopwatches/strict_stopwatch.hpp>

namespace ex
{
  template <long long speed>
  struct cycle_count
  {
      typedef typename boost::ratio_multiply<boost::ratio<speed>, boost::mega>::type frequency;  // Mhz
      typedef typename boost::ratio_divide<boost::ratio<1>, frequency>::type period;
      typedef long long rep;
      typedef boost::chrono::duration<rep, period> duration;
      typedef boost::chrono::time_point<cycle_count> time_point;
      BOOST_STATIC_CONSTEXPR bool is_steady =             true;
      static long long ticks_;

      static time_point now()
      {
          // return exact cycle count
          return time_point(duration(ticks_));
      }
      static time_point now(boost::system::error_code & )
      {
          // return exact cycle count
        return time_point(duration(ticks_));
      }
      static void advance(std::size_t ticks)
      {
        ticks_ += ticks;
      }
      template <typename D>
      static void advance(D const& d)
      {
        ticks_ += boost::chrono::ceil<duration>(d).count();
      }
  };
  template <long long speed>
  long long cycle_count<speed>::ticks_ = 0;


  template<class Clock, class Rep, class Period>
  void sleep_for(const boost::chrono::duration<Rep, Period>& d)
  {
    Clock::advance(d);
  }

}


namespace boost
{
  namespace chrono
  {

    template <typename CharT, long long speed>
    struct basic_stopwatch_reporter_default_formatter<CharT, strict_stopwatch<ex::cycle_count<speed> > >
    {
      typedef basic_elapsed_formatter<milli, CharT> type;
    };

//    template <long long speed>
//    struct wstopwatch_reporter_default_formatter<strict_stopwatch<ex::cycle_count<speed> > >
//    {
//      typedef welapsed_formatter type;
//    };

  } // namespace chrono
} // namespace boost


#endif
