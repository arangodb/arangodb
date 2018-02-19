//  Copyright 2010-2011 Vicente J. Botet Escriba
//  Copyright (c) Microsoft Corporation 2014
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#define BOOST_CHRONO_VERSION 2

#include <iostream>
#include <boost/type_traits/is_same.hpp>
#include <boost/chrono/stopwatches/strict_stopwatch.hpp>
#include "../cycle_count.hpp"
#include <boost/chrono/stopwatches/reporters/stopwatch_reporter.hpp>
#include <boost/chrono/stopwatches/reporters/system_default_formatter.hpp>

#include <boost/chrono/chrono_io.hpp>
#include <boost/system/system_error.hpp>
#include <boost/detail/lightweight_test.hpp>

#if !defined(BOOST_NO_CXX11_STATIC_ASSERT)
#define NOTHING ""
#endif

using namespace boost::chrono;


template <typename Reporter>
void check_invariants()
{
  typedef Reporter Stopwatch;
    BOOST_CHRONO_STATIC_ASSERT((boost::is_same<typename Stopwatch::rep, typename Stopwatch::clock::duration::rep>::value), NOTHING, ());
    BOOST_CHRONO_STATIC_ASSERT((boost::is_same<typename Stopwatch::period, typename Stopwatch::clock::duration::period>::value), NOTHING, ());
    BOOST_CHRONO_STATIC_ASSERT((boost::is_same<typename Stopwatch::duration, typename Stopwatch::clock::time_point::duration>::value), NOTHING, ());
    BOOST_CHRONO_STATIC_ASSERT(Stopwatch::is_steady == Stopwatch::clock::is_steady, NOTHING, ());
    BOOST_CHRONO_STATIC_ASSERT((boost::is_same<typename Stopwatch::stopwatch_type, strict_stopwatch<typename Stopwatch::clock> >::value), NOTHING, ());
}

template <typename Reporter>
void check_default_constructor()
{
  Reporter _;
}

struct file_line {
  file_line(const char* file, std::size_t line)
  : fmt("%1%[%2%] Elapsed time:")
  {
    fmt % file % line;
  }
  ~file_line()
  {
    std::cout << fmt;
  }
  boost::format fmt;

};

template <typename Reporter>
void check_file_line2()
{
  Reporter _("%1%\n");
  file_line fl(__FILE__, __LINE__);
  ex::sleep_for<typename Reporter::clock>(milliseconds(100));

}
template <typename Reporter>
void check_file_line()
{
  Reporter rp("%1%[%2%] Elapsed time: %3%\n");
  rp.format() % __FILE__ % __LINE__;

  ex::sleep_for<typename Reporter::clock>(milliseconds(100));

}


#if !defined BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING
template <typename Reporter>
void check_constructor_ec()
{
  boost::system::error_code ec;
  Reporter _(ec);
  BOOST_TEST(ec.value()==0);
}

template <typename Reporter>
void check_constructor_throws()
{
  Reporter _(boost::throws());
}
#endif

template <typename Reporter>
void check_elapsed(bool check=true)
{
  Reporter sw;
  ex::sleep_for<typename Reporter::clock>(milliseconds(100));
  typename Reporter::duration d=sw.elapsed();
  std::cout << d << std::endl;
  if (check)
    BOOST_TEST(d >= milliseconds(100));
}

template <typename Reporter>
void check_report()
{
  Reporter sw;
  ex::sleep_for<typename Reporter::clock>(milliseconds(100));
  sw.report();
}

template <typename Clock>
void check_all(bool check=true)
{
  typedef stopwatch_reporter<strict_stopwatch<Clock> > Reporter;
  typedef stopwatch_reporter<strict_stopwatch<Clock>, elapsed_formatter > ReporterE;

  check_invariants<Reporter>();
  check_default_constructor<Reporter>();
#if !defined BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING
  check_constructor_ec<Reporter>();
  check_constructor_throws<Reporter>();
#endif
  check_elapsed<Reporter>(check);
  check_report<Reporter>();
  check_file_line<ReporterE>();
}

int main()
{
  typedef strict_stopwatch<high_resolution_clock > Stopwatch;
  typedef basic_stopwatch_reporter_default_formatter<char, Stopwatch>::type Formatter;
  typedef stopwatch_reporter<Stopwatch> Reporter;
  static Formatter fmtr;

  Reporter _(fmtr);

  check_all<ex::cycle_count<1500> >(true);

#if 0
  std::cout << "high_resolution_clock=\n";

  check_all<high_resolution_clock>();
#ifdef BOOST_CHRONO_HAS_CLOCK_STEADY
  std::cout << "steady_clock=\n";
  check_all<steady_clock>(false);
#endif
  std::cout << "system_clock=\n";
  check_all<system_clock>(false);

#if defined(BOOST_CHRONO_HAS_THREAD_CLOCK)
  std::cout << "thread_clock=\n";
  check_all<thread_clock>(false);
#endif

#if defined(BOOST_CHRONO_HAS_PROCESS_CLOCKS)
  std::cout << "process_real_cpu_clock=\n";
  check_all<process_real_cpu_clock>(false);
#if ! BOOST_OS_WINDOWS || BOOST_PLAT_WINDOWS_DESKTOP
  std::cout << "process_user_cpu_clock=\n";
  check_all<process_user_cpu_clock>(false);
  std::cout << "process_system_cpu_clock=\n";
  check_all<process_system_cpu_clock>(false);
  std::cout << "process_cpu_clock=\n";
  check_all<process_cpu_clock>(false);
#endif
#endif
#endif

  return boost::report_errors();
}
