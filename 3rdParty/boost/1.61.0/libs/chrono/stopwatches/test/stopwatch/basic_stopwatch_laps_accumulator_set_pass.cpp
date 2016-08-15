//  Copyright 2010-2011 Vicente J. Botet Escriba
//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

#define BOOST_CHRONO_VERSION 2

#include <iostream>
#include <boost/type_traits/is_same.hpp>
#include <boost/chrono/stopwatches/stopwatch.hpp>
#include <boost/chrono/stopwatches/collectors/laps_accumulator_set.hpp>
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
  BOOST_TEST(sw.get_laps_collector().last()==Stopwatch::duration::zero());
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==0);
}

template <typename Stopwatch>
void check_dont_start_constructor()
{
  Stopwatch sw(boost::chrono::dont_start);
  BOOST_TEST(!sw.is_running());
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  typename Stopwatch::duration d=sw.elapsed();
  BOOST_TEST(d == Stopwatch::duration::zero());
  BOOST_TEST(sw.get_laps_collector().last()==Stopwatch::duration::zero());
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==0);
}

#if !defined BOOST_CHRONO_DONT_PROVIDE_HYBRID_ERROR_HANDLING
template <typename Stopwatch>
void check_constructor_ec()
{
  boost::system::error_code ec;
  Stopwatch sw(ec);
  BOOST_TEST(sw.is_running());
  BOOST_TEST(ec.value()==0);
  BOOST_TEST(sw.get_laps_collector().last()==Stopwatch::duration::zero());
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==0);
}

template <typename Stopwatch>
void check_constructor_throws()
{
  Stopwatch sw(boost::throws());
  BOOST_TEST(sw.is_running());
  BOOST_TEST(sw.get_laps_collector().last()==Stopwatch::duration::zero());
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==0);
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
  BOOST_TEST(sw.get_laps_collector().last()==Stopwatch::duration::zero());
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==0);
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
  BOOST_TEST(sw.get_laps_collector().last()==Stopwatch::duration::zero());
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==0);
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
  BOOST_TEST(sw.get_laps_collector().last()==Stopwatch::duration::zero());
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==0);
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
  BOOST_TEST(d == boost::chrono::milliseconds(100));
  BOOST_TEST(sw.get_laps_collector().last()==boost::chrono::milliseconds(100));
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==1);
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
  BOOST_TEST(d == boost::chrono::milliseconds(100));
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==1);
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
  BOOST_TEST(d == boost::chrono::milliseconds(100));
  BOOST_TEST(sw.get_laps_collector().last()==boost::chrono::milliseconds(100));
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==1);
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
  BOOST_TEST(d == boost::chrono::milliseconds(100));
  BOOST_TEST(sw.get_laps_collector().last()==boost::chrono::milliseconds(100));
  ex::sleep_for<typename Stopwatch::clock>(boost::chrono::milliseconds(100));
  sw.stop();
  BOOST_TEST(!sw.is_running());
  d=sw.elapsed();
  BOOST_TEST(!sw.is_running());
  BOOST_TEST(d == boost::chrono::milliseconds(100));
  BOOST_TEST(sw.get_laps_collector().last()==boost::chrono::milliseconds(100));
  BOOST_TEST(boost::accumulators::count(sw.get_laps_collector().accumulator_set())==1);
}


template <typename Stopwatch>
void check_all()
{
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

}


int main()
{
  std::cout << "cycle_count=";
  check_all<boost::chrono::stopwatch< ex::cycle_count<1500>, boost::chrono::laps_accumulator_set<ex::cycle_count<1500>::duration> > >();
  return boost::report_errors();
}
