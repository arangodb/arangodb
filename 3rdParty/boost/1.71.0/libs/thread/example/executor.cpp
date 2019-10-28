// Copyright (C) 2012-2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif
#include <iostream>

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
//#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID
#define BOOST_THREAD_QUEUE_DEPRECATE_OLD

#include <boost/thread/caller_context.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executors/loop_executor.hpp>
#include <boost/thread/executors/serial_executor.hpp>
#include <boost/thread/executors/inline_executor.hpp>
#include <boost/thread/executors/thread_executor.hpp>
#include <boost/thread/executors/executor.hpp>
#include <boost/thread/executors/executor_adaptor.hpp>
#include <boost/thread/executor.hpp>
#include <boost/thread/future.hpp>
#include <boost/assert.hpp>
#include <string>
#include <iostream>
#include <cassert>

boost::future<void> p(boost::future<void> f) {
    assert(f.is_ready());
    return boost::make_ready_future();
}

void p1()
{
  // std::cout << BOOST_CONTEXTOF << std::endl;
  //boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
}

void p2()
{
  // std::cout << BOOST_CONTEXTOF << std::endl;
  //boost::this_thread::sleep_for(boost::chrono::seconds(10));
}

int f1()
{
  std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::seconds(1));
  std::cout << BOOST_CONTEXTOF << std::endl;
  return 1;
}
int f2(int i)
{
  // std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::seconds(2));
  return i + 1;
}

void submit_some(boost::executor& tp)
{
  for (int i = 0; i < 3; ++i) {
    tp.submit(&p2);
  }
  for (int i = 0; i < 3; ++i) {
    tp.submit(&p1);
  }

}


void at_th_entry(boost::basic_thread_pool& )
{

}

int test_executor_adaptor()
{
  std::cout << BOOST_CONTEXTOF << std::endl;
  {
    try
    {
      {
        boost::executor_adaptor < boost::basic_thread_pool > ea(4);
        std::cout << BOOST_CONTEXTOF << std::endl;
        submit_some( ea);
        std::cout << BOOST_CONTEXTOF << std::endl;
#if 1
      // fixme
      // ERROR= tr1::bad_weak_ptr
        {
          boost::future<int> t1 = boost::async(ea, &f1);
          boost::future<int> t2 = boost::async(ea, &f1);
          std::cout << BOOST_CONTEXTOF << " t1= " << t1.get() << std::endl;
          std::cout << BOOST_CONTEXTOF << " t2= " << t2.get() << std::endl;
        }
        std::cout << BOOST_CONTEXTOF << std::endl;
        submit_some(ea);
        std::cout << BOOST_CONTEXTOF << std::endl;
        {
          boost::basic_thread_pool ea3(1);
          std::cout << BOOST_CONTEXTOF << std::endl;
          boost::future<int> t1 = boost::async(ea3, &f1);
          std::cout << BOOST_CONTEXTOF << std::endl;
          boost::future<int> t2 = boost::async(ea3, &f1);
          std::cout << BOOST_CONTEXTOF << std::endl;
          //boost::future<int> t2 = boost::async(ea3, f2, 1); // todo this doesn't compiles yet on C++11
          //boost::future<int> t2 = boost::async(ea3, boost::bind(f2, 1)); // todo this doesn't compiles yet on C++98
          std::cout << BOOST_CONTEXTOF << " t1= " << t1.get() << std::endl;
          std::cout << BOOST_CONTEXTOF << " t2= " << t2.get() << std::endl;
        }
#endif
        std::cout << BOOST_CONTEXTOF << std::endl;
        submit_some(ea);
        std::cout << BOOST_CONTEXTOF << std::endl;
      }
      std::cout << BOOST_CONTEXTOF << std::endl;
      {
        boost::executor_adaptor < boost::loop_executor > ea2;
        submit_some( ea2);
        ea2.underlying_executor().run_queued_closures();
      }
#if ! defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
      std::cout << BOOST_CONTEXTOF << std::endl;
      {
        boost::executor_adaptor < boost::basic_thread_pool > ea1(4);
        boost::executor_adaptor < boost::serial_executor > ea2(ea1);
        submit_some(ea2);
      }
#endif
      std::cout << BOOST_CONTEXTOF << std::endl;
      {
        boost::executor_adaptor < boost::inline_executor > ea1;
        submit_some(ea1);
      }
      std::cout << BOOST_CONTEXTOF << std::endl;
      {
        boost::executor_adaptor < boost::thread_executor > ea1;
        submit_some(ea1);
      }
      std::cout << BOOST_CONTEXTOF << std::endl;
#if 1
      // fixme
      // ERROR= tr1::bad_weak_ptr
      {
        boost::basic_thread_pool  ea(4, at_th_entry);
        boost::future<int> t1 = boost::async(ea, &f1);
        std::cout << BOOST_CONTEXTOF << " t1= " << t1.get() << std::endl;
      }
#endif
      std::cout << BOOST_CONTEXTOF << std::endl;
      {
        boost::async(&f1);
      }
#if 1
      // fixme
      // ERROR= tr1::bad_weak_ptr

      std::cout << BOOST_CONTEXTOF << std::endl;
      {
        boost::basic_thread_pool  ea(1);
        boost::async(ea,&f1);
      }
#endif
      std::cout << BOOST_CONTEXTOF << std::endl;
      boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
      std::cout << BOOST_CONTEXTOF << std::endl;
    }
    catch (std::exception& ex)
    {
      std::cout << "ERROR= " << ex.what() << "" << std::endl;
      return 1;
    }
    catch (...)
    {
      std::cout << " ERROR= exception thrown" << std::endl;
      return 2;
    }
  }
  // std::cout << BOOST_CONTEXTOF << std::endl;
  return 0;
}


int main()
{
  return test_executor_adaptor();

#if 0 && defined BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION \
  && defined BOOST_THREAD_PROVIDES_EXECUTORS \
  &&  ! defined BOOST_NO_CXX11_RVALUE_REFERENCES

  boost::basic_thread_pool executor;
  // compiles
  boost::make_ready_future().then(&p);

  // ??
  boost::make_ready_future().then(executor, &p);

  // doesn't compile
  boost::make_ready_future().then(executor, &p);
#endif
}
