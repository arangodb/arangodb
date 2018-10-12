// Copyright (C) 2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif
#include <boost/thread/detail/log.hpp>
#include <boost/thread/executors/loop_executor.hpp>
#include <boost/assert.hpp>
#include <boost/thread/thread_only.hpp>
#include <string>

#ifdef BOOST_MSVC
#pragma warning(disable: 4127) // conditional expression is constant
#endif

void p1()
{
  BOOST_THREAD_LOG
    << boost::this_thread::get_id()  << " P1" << BOOST_THREAD_END_LOG;
}

void p2()
{
  BOOST_THREAD_LOG
    << boost::this_thread::get_id()  << " P2" << BOOST_THREAD_END_LOG;
}

void submit_some(boost::loop_executor& tp) {
  tp.submit(&p1);
  tp.submit(&p2);
  tp.submit(&p1);
  tp.submit(&p2);
  tp.submit(&p1);
  tp.submit(&p2);
  tp.submit(&p1);
  tp.submit(&p2);
  tp.submit(&p1);
  tp.submit(&p2);
}

int main()
{
  BOOST_THREAD_LOG
    << boost::this_thread::get_id()  << " <MAIN" << BOOST_THREAD_END_LOG;
  {
    try
    {
      boost::loop_executor tp;
      submit_some(tp);
      tp.run_queued_closures();
      submit_some(tp);
      tp.run_queued_closures();
    }
    catch (std::exception& ex)
    {
      BOOST_THREAD_LOG
        << "ERRORRRRR " << ex.what() << "" << BOOST_THREAD_END_LOG;
      return 1;
    }
    catch (...)
    {
      BOOST_THREAD_LOG
        << " ERRORRRRR exception thrown" << BOOST_THREAD_END_LOG;
      return 2;
    }
  }

  BOOST_THREAD_LOG
    << boost::this_thread::get_id()  << "MAIN>" << BOOST_THREAD_END_LOG;
  return 0;
}
