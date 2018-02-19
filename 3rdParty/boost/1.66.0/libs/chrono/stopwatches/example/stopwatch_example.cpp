//  example/stopwatch_example.cpp  ---------------------------------------------------//
//  Copyright Beman Dawes 2006, 2008
//  Copyright 2009-2011 Vicente J. Botet Escriba
//  Copyright (c) Microsoft Corporation 2014
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt
//  See http://www.boost.org/libs/chrono/stopwatches for documentation.

#include <iostream>
#include <boost/chrono/stopwatches/strict_stopwatch.hpp>
#include <boost/chrono/chrono_io.hpp>
#include <boost/chrono/process_cpu_clocks.hpp>
#include <cmath>

using namespace boost::chrono;

#ifdef BOOST_CHRONO_HAS_PROCESS_CLOCKS
#if ! BOOST_OS_WINDOWS || BOOST_PLAT_WINDOWS_DESKTOP
typedef process_cpu_clock clock_type;
#else
// Windows store doesn't support process_cpu_clock, default to high_resolution_clock.
typedef high_resolution_clock clock_type;
#endif
#else
typedef high_resolution_clock clock_type;
#endif

namespace ex
{
    template<class Rep, class Period>
    void sleep_for(const duration<Rep, Period>& d)
    {
      typedef high_resolution_clock Clock;
      typename Clock::time_point go =
          Clock::now() + d;
      while (Clock::now() < go)
      {
      }
    }
}

int f1(long j)
{
  strict_stopwatch<clock_type> sw;

  for ( long i = 0; i < j; ++i )
    std::sqrt( 123.456L );  // burn some time
  ex::sleep_for(milliseconds(100));

  std::cout << "f1("<< j <<") Elapsed time: " << sw.elapsed() << std::endl;
  return 0;
}
int main()
{
  strict_stopwatch<clock_type> sw;

  f1(1000);
  f1(2000);
  f1(3000);
#ifdef BOOST_CHRONO_HAS_PROCESS_CLOCKS2
  std::cout << "main() Elapsed time: " << duration_cast<duration<process_times<double>,boost::ratio<1> > >(sw.elapsed()) << std::endl;
  std::cout << "main() Elapsed time: " << duration_cast<duration<process_times<nanoseconds::rep>,boost::milli> >(sw.elapsed()) << std::endl;
#endif
  std::cout << "main() Elapsed time: " << sw.elapsed() << std::endl;
  return 0;
}
