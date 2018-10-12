//  Copyright 2010-2011 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#define BOOST_CHRONO_VERSION 2

#include <iostream>
#include <boost/type_traits/is_same.hpp>
#include <boost/chrono/stopwatches/suspendable_stopwatch.hpp>
#include "../cycle_count.hpp"
#include <boost/detail/lightweight_test.hpp>

#if !defined(BOOST_NO_CXX11_STATIC_ASSERT)
#define NOTHING ""
#endif


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


template <typename Stopwatch>
void check_start_suspend()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.suspend();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(sw.is_suspended());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(sw.is_suspended());
  BOOST_TEST(d >= boost::chrono::milliseconds(100));
  BOOST_TEST(d < boost::chrono::milliseconds(200));
}

template <typename Stopwatch>
void check_start_suspend_stop()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.suspend();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(sw.is_suspended());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(sw.is_suspended());
  BOOST_TEST(d >= boost::chrono::milliseconds(100));
  BOOST_TEST(d < boost::chrono::milliseconds(200));
  sw.stop();
  d=sw.elapsed();
  BOOST_TEST(!sw.is_running());
  BOOST_TEST(!sw.is_suspended());
  BOOST_TEST(d == boost::chrono::milliseconds(0));
}

template <typename Stopwatch>
void check_start_suspend_resume()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.suspend();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(sw.is_suspended());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.resume();
  BOOST_TEST(sw.is_running());
  BOOST_TEST(!sw.is_suspended());
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(d >= boost::chrono::milliseconds(100));
  BOOST_TEST(d < boost::chrono::milliseconds(200));
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  d=sw.elapsed();
  BOOST_TEST(d >= boost::chrono::milliseconds(200));
  BOOST_TEST(d < boost::chrono::milliseconds(300));
}

template <typename Stopwatch>
void check_start_scoped_suspend()
{
  Stopwatch sw;
  BOOST_TEST(sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  {
    typename Stopwatch::scoped_suspend _(sw);
    BOOST_TEST(sw.is_running());
    BOOST_TEST(sw.is_suspended());
    ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  }
  BOOST_TEST(sw.is_running());
  BOOST_TEST(!sw.is_suspended());
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(d >= boost::chrono::milliseconds(100));
  BOOST_TEST(d < boost::chrono::milliseconds(200));
}
template <typename Clock>
void check_all()
{
  typedef boost::chrono::suspendable_stopwatch<Clock> Stopwatch;
  check_invariants<Stopwatch>();
  check_default_constructor<Stopwatch>();
#if !defined BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING
  check_constructor_ec<Stopwatch>();
  check_constructor_throws<Stopwatch>();
#endif
  check_elapsed<Stopwatch>();

  check_start_start<Stopwatch>();
  check_dont_start_constructor<Stopwatch>();
  check_dont_start_start<Stopwatch>();
  check_dont_start_start_stop<Stopwatch>();
  check_dont_start_scoped_run<Stopwatch>();
  check_stop<Stopwatch>();
  check_stop_stop<Stopwatch>();

  check_start_suspend<Stopwatch>();
  check_start_suspend_resume<Stopwatch>();
  check_start_suspend_stop<Stopwatch>();
  check_start_scoped_suspend<Stopwatch>();

}


int main()
{
  std::cout << "cycle_count=";
  check_all<ex::cycle_count<1500> >();

  return boost::report_errors();
}
