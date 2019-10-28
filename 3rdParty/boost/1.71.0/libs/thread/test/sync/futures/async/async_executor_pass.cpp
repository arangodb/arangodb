//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2011 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/future.hpp>

// template <class Executor, class F, class... Args>
//     future<typename result_of<F(Args...)>::type>
//     async(Executor& ex, F&& f, Args&&... args);

#define BOOST_THREAD_VERSION 5
#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif
#include <iostream>
#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/detail/memory.hpp>
#include <boost/thread/csbl/memory/unique_ptr.hpp>
#include <memory>
#include <boost/detail/lightweight_test.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executor.hpp>

typedef boost::chrono::high_resolution_clock Clock;
typedef boost::chrono::milliseconds ms;

class A
{
  long data_;

public:
  typedef long result_type;

  explicit A(long i) :
    data_(i)
  {
  }

  long doit() const
  {
    boost::this_thread::sleep_for(ms(200));
    return data_;
  }
  long operator()() const
  {
    boost::this_thread::sleep_for(ms(200));
    return data_;
  }
};

class MoveOnly
{
public:
  typedef int result_type;

  int value;

BOOST_THREAD_MOVABLE_ONLY(MoveOnly)
  MoveOnly()
  {
    value = 0;
  }
  MoveOnly( BOOST_THREAD_RV_REF(MoveOnly))
      {
        value = 1;
      }
      MoveOnly& operator=(BOOST_THREAD_RV_REF(MoveOnly))
      {
        value = 2;
        return *this;
      }

      int operator()()
      {
        boost::this_thread::sleep_for(ms(200));
        return 3;
      }
      template <typename OS>
      friend OS& operator<<(OS& os, MoveOnly const& v)
      {
        os << v.value;
        return os;
      }
    };

    namespace boost
    {
BOOST_THREAD_DCL_MOVABLE    (MoveOnly)
  }

int f0()
{
  boost::this_thread::sleep_for(ms(200));
  return 3;
}

int i = 0;

int& f1()
{
  boost::this_thread::sleep_for(ms(200));
  return i;
}

void f2()
{
  boost::this_thread::sleep_for(ms(200));
}

boost::csbl::unique_ptr<int> f3_0()
{
  boost::this_thread::sleep_for(ms(200));
  boost::csbl::unique_ptr<int> r( (new int(3)));
  return boost::move(r);
}
MoveOnly f3_1()
{
  boost::this_thread::sleep_for(ms(200));
  MoveOnly r;
  return boost::move(r);
}

boost::csbl::unique_ptr<int> f3(int i)
{
  boost::this_thread::sleep_for(ms(200));
  return boost::csbl::unique_ptr<int>(new int(i));
}

boost::csbl::unique_ptr<int> f4(
    BOOST_THREAD_RV_REF_BEG boost::csbl::unique_ptr<int> BOOST_THREAD_RV_REF_END p
)
{
  boost::this_thread::sleep_for(ms(200));
  return boost::move(p);
}

struct check_timer {
  boost::chrono::nanoseconds delay;
  Clock::time_point start;
  check_timer(boost::chrono::nanoseconds delay)
  : delay(delay)
  , start(Clock::now())
  {
  }
  ~check_timer() {
    Clock::time_point now = Clock::now();
    BOOST_TEST(now - start < delay);
    std::cout << __FILE__ << "[" << __LINE__ << "] " << (now - start).count() << std::endl;
  }

};

int main()
{
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
#if defined BOOST_THREAD_PROVIDES_EXECUTORS
  {
    try
    {
      boost::executor_adaptor<boost::basic_thread_pool> ex(1);
      boost::future<int> f = boost::async(ex, &f0);
      boost::this_thread::sleep_for(ms(300));
      int res;
      {
        check_timer timer(ms(500));
        res = f.get();
      }
      BOOST_TEST(res == 3);
    }
    catch (std::exception& ex)
    {
      std::cout << __FILE__ << "[" << __LINE__ << "]" << ex.what() << std::endl;
      BOOST_TEST(false && "exception thrown");
    }
    catch (...)
    {
      BOOST_TEST(false && "exception thrown");
    }
  }
#endif
#if defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD) && defined BOOST_THREAD_PROVIDES_EXECUTORS
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  {
    try
    {
      boost::executor_adaptor<boost::basic_thread_pool> ex(1);
      boost::future<long> f = boost::async(ex, A(3));
      boost::this_thread::sleep_for(ms(300));
      int res;
      {
        check_timer timer(ms(500));
        res = f.get();
      }
      BOOST_TEST(res == 3);
    }
    catch (std::exception& ex)
    {
      std::cout << __FILE__ << "[" << __LINE__ << "]" << ex.what() << std::endl;
      BOOST_TEST(false && "exception thrown");
    }
    catch (...)
    {
      BOOST_TEST(false && "exception thrown");
    }

  }
#endif
#if defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD) && defined BOOST_THREAD_PROVIDES_EXECUTORS
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  {
    try
    {
      boost::executor_adaptor<boost::basic_thread_pool> ex(1);
      MoveOnly mo;
      boost::future<int> f = boost::async(ex, boost::move(mo));
      //boost::future<int> f = boost::async(ex, MoveOnly());
      boost::this_thread::sleep_for(ms(300));
      int res;
      {
        check_timer timer(ms(500));
        res = f.get();
      }
      BOOST_TEST(res == 3);
    }
    catch (std::exception& ex)
    {
      std::cout << __FILE__ << "[" << __LINE__ << "]" << ex.what() << std::endl;
      BOOST_TEST(false && "exception thrown");
    }
    catch (...)
    {
      BOOST_TEST(false && "exception thrown");
    }
  }
#endif

  return boost::report_errors();
}
