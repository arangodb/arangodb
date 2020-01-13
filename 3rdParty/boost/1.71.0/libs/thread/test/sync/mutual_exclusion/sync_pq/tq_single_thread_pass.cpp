// Copyright (C) 2014 Ian Forbed
// Copyright (C) 2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/function.hpp>
#include <boost/thread/concurrent_queues/sync_timed_queue.hpp>
#include <boost/thread/executors/work.hpp>

#include <boost/core/lightweight_test.hpp>

using namespace boost::chrono;

typedef boost::concurrent::sync_timed_queue<int> sync_tq;

void test_all()
{
  sync_tq pq;
  BOOST_TEST(pq.empty());
  BOOST_TEST(!pq.closed());
  BOOST_TEST_EQ(pq.size(), std::size_t(0));

  for(int i = 1; i <= 5; i++){
    pq.push(i, milliseconds(i*100));
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(i));
  }

  for(int i = 6; i <= 10; i++){
    pq.push(i,steady_clock::now() + milliseconds(i*100));
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(i));
  }

  for(int i = 1; i <= 10; i++){
    int val = pq.pull();
    BOOST_TEST_EQ(val, i);
  }

  int val;
  boost::queue_op_status st = pq.nonblocking_pull(val);
  BOOST_TEST(boost::queue_op_status::empty == st);

  BOOST_TEST(pq.empty());
  pq.close();
  BOOST_TEST(pq.closed());
}

void test_all_with_try()
{
  sync_tq pq;
  BOOST_TEST(pq.empty());
  BOOST_TEST(!pq.closed());
  BOOST_TEST_EQ(pq.size(), std::size_t(0));

  for(int i = 1; i <= 5; i++){
    boost::queue_op_status st = pq.try_push(i, milliseconds(i*100));
    BOOST_TEST(st == boost::queue_op_status::success );
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(i));
  }

  for(int i = 6; i <= 10; i++){
    boost::queue_op_status st = pq.try_push(i,steady_clock::now() + milliseconds(i*100));
    BOOST_TEST(st == boost::queue_op_status::success );
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(i));
  }

  for(int i = 1; i <= 10; i++){
    int val=0;
    boost::queue_op_status st = pq.wait_pull(val);
    BOOST_TEST(st == boost::queue_op_status::success );
    BOOST_TEST_EQ(val, i);
  }

  int val;
  boost::queue_op_status st = pq.nonblocking_pull(val);
  BOOST_TEST(st == boost::queue_op_status::empty );

  BOOST_TEST(pq.empty());
  pq.close();
  BOOST_TEST(pq.closed());
}

void func(steady_clock::time_point pushed, steady_clock::duration dur)
{
    BOOST_TEST(pushed + dur <= steady_clock::now());
}
void func2()
{
    BOOST_TEST(false);
}

/**
 * This test ensures that when items come of the front of the queue
 * that at least $dur has elapsed.
 */
void test_deque_times()
{
    boost::concurrent::sync_timed_queue<boost::function<void()> > tq;
    for(int i = 0; i < 10; i++)
    {
        steady_clock::duration d = milliseconds(i*100);
        boost::function<void()> fn = boost::bind(func, steady_clock::now(), d);
        tq.push(fn, d);
    }
    while(!tq.empty())
    {
        boost::function<void()> fn = tq.pull();
        fn();
    }
}

/**
 * This test ensures that when items come of the front of the queue
 * that at least $dur has elapsed.
 */
#if 0
void test_deque_times2()
{
    boost::concurrent::sync_timed_queue<boost::executors::work> tq;
    for(int i = 0; i < 10; i++)
    {
        steady_clock::duration d = milliseconds(i*100);
        tq.push(func2, d);
    }
    while(!tq.empty())
    {
        boost::executors::work fn = tq.pull();
        fn();
    }
}
#endif

int main()
{
  test_all();
  test_all_with_try();
  test_deque_times();
  //test_deque_times2(); // rt fails
  return boost::report_errors();
}
