//  Copyright 2010-2011 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#define BOOST_CHRONO_VERSION 2

#include <iostream>
#include <boost/type_traits/is_same.hpp>
#include <boost/chrono/stopwatches/reporters/stopclock.hpp>
#include "../cycle_count.hpp"
#include <boost/chrono/stopwatches/reporters/system_default_formatter.hpp>

#include <boost/chrono/chrono_io.hpp>
#include <boost/system/system_error.hpp>
#include <boost/detail/lightweight_test.hpp>

#if !defined(BOOST_NO_CXX11_STATIC_ASSERT)
#define NOTHING ""
#endif

using namespace boost::chrono;


template <typename Stopwatch>
void check_invariants()
{
    BOOST_CHRONO_STATIC_ASSERT((boost::is_same<typename Stopwatch::rep, typename Stopwatch::clock::duration::rep>::value), NOTHING, ());
    BOOST_CHRONO_STATIC_ASSERT((boost::is_same<typename Stopwatch::period, typename Stopwatch::clock::duration::period>::value), NOTHING, ());
    BOOST_CHRONO_STATIC_ASSERT((boost::is_same<typename Stopwatch::duration, typename Stopwatch::clock::time_point::duration>::value), NOTHING, ());
    BOOST_CHRONO_STATIC_ASSERT(Stopwatch::is_steady == Stopwatch::clock::is_steady, NOTHING, ());
}

template <typename Stopwatch>
void check_default_constructor()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
}

template <typename Stopwatch>
void check_dont_start_constructor()
{
  Stopwatch sw(boost::chrono::dont_start);
  BOOST_TEST(!sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(d == Stopwatch::duration::zero());
}

#if !defined BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING
template <typename Stopwatch>
void check_constructor_ec()
{
  boost::system::error_code ec;
  Stopwatch sw(ec);
  BOOST_TEST(sw.is_running());
  BOOST_TEST(ec.value()==0);
}

template <typename Stopwatch>
void check_constructor_throws()
{
  Stopwatch sw(boost::throws());
  BOOST_TEST(sw.is_running());
}
#endif

template <typename Stopwatch>
void check_elapsed()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(d >= boost::chrono::milliseconds(100));
}

template <typename Stopwatch>
void check_start_start()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.start();
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(d >= boost::chrono::milliseconds(100));
  BOOST_TEST(d < boost::chrono::milliseconds(200));
}

template <typename Stopwatch>
void check_dont_start_start()
{
  Stopwatch sw(boost::chrono::dont_start);
  BOOST_TEST(!sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.start();
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(d >= boost::chrono::milliseconds(100));
  BOOST_TEST(d < boost::chrono::milliseconds(200));
}

template <typename Stopwatch>
void check_dont_start_start_stop()
{
  Stopwatch sw(boost::chrono::dont_start);
  BOOST_TEST(!sw.is_running());
  sw.start();
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.stop();
  BOOST_TEST(!sw.is_running());
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(!sw.is_running());
  BOOST_TEST(d >= boost::chrono::milliseconds(0));
}

template <typename Stopwatch>
void check_dont_start_scoped_run()
{
  Stopwatch sw(boost::chrono::dont_start);
  BOOST_TEST(!sw.is_running());
  {
    typename Stopwatch::scoped_run _(sw);
    BOOST_TEST(sw.is_running());
    ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  }
  BOOST_TEST(!sw.is_running());
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(!sw.is_running());
  BOOST_TEST(d >= boost::chrono::milliseconds(0));
}

template <typename Stopwatch>
void check_stop()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.stop();
  BOOST_TEST(!sw.is_running());
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(!sw.is_running());
  BOOST_TEST(d == boost::chrono::milliseconds(0));
}

template <typename Stopwatch>
void check_stop_stop()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.stop();
  BOOST_TEST(!sw.is_running());
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(!sw.is_running());
  BOOST_TEST(d == boost::chrono::milliseconds(0));
  sw.stop();
  BOOST_TEST(!sw.is_running());
  d=sw.elapsed();
  BOOST_TEST(!sw.is_running());
  BOOST_TEST(d == boost::chrono::milliseconds(0));
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

template <typename Reporter>
void check_report()
{
  Reporter sw;
  ex::sleep_for<typename Reporter::clock>(milliseconds(100));
  sw.report();
}




template <typename Clock>
void check_all()
{
  typedef stopclock<Clock> Reporter;
  typedef stopclock<Clock, no_memory<typename Clock::duration>, elapsed_formatter > ReporterE;

  check_invariants<Reporter>();
  check_default_constructor<Reporter>();
#if !defined BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING
  check_constructor_ec<Reporter>();
  check_constructor_throws<Reporter>();
#endif
  check_elapsed<Reporter>();

  check_report<Reporter>();
  check_file_line<ReporterE>();

  check_start_start<Reporter>();
  check_dont_start_constructor<Reporter>();
  check_dont_start_start<Reporter>();
  check_dont_start_start_stop<Reporter>();
  check_dont_start_scoped_run<Reporter>();
  check_stop<Reporter>();
  check_stop_stop<Reporter>();


}

int main()
{
  typedef stopclock<high_resolution_clock > Reporter;

  static Reporter::formatter_type fmtr;

  //Reporter _(fmtr);
  Reporter _;

  check_all<ex::cycle_count<1500> >();

  return boost::report_errors();
}
