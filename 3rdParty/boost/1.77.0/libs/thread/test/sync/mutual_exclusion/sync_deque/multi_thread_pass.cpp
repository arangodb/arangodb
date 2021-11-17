// Copyright (C) 2013 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/concurrent_queues/sync_deque.hpp>

// class sync_deque<T>

//    push || pull;

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4

#include <boost/thread/concurrent_queues/sync_deque.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/barrier.hpp>

#include <boost/detail/lightweight_test.hpp>

template <typename ValueType>
struct call_push_back
{
  boost::sync_deque<ValueType> *q_;
  boost::barrier *go_;

  call_push_back(boost::sync_deque<ValueType> *q, boost::barrier *go) :
    q_(q), go_(go)
  {
  }
  typedef void result_type;
  void operator()()
  {
    go_->count_down_and_wait();
    q_->push_back(42);
  }
};

template <typename ValueType>
struct call_pull_front
{
  boost::sync_deque<ValueType> *q_;
  boost::barrier *go_;

  call_pull_front(boost::sync_deque<ValueType> *q, boost::barrier *go) :
    q_(q), go_(go)
  {
  }
  typedef ValueType result_type;
  ValueType operator()()
  {
    go_->count_down_and_wait();
    return q_->pull_front();
  }
};

template <typename ValueType>
struct call_wait_pull_front
{
  boost::sync_deque<ValueType> *q_;
  boost::barrier *go_;

  call_wait_pull_front(boost::sync_deque<ValueType> *q, boost::barrier *go) :
    q_(q), go_(go)
  {
  }
  typedef boost::queue_op_status result_type;
  boost::queue_op_status operator()(ValueType& v)
  {
    go_->wait();
    return q_->wait_pull_front(v);
  }
};

void test_concurrent_push_back_and_pull_front_on_empty_queue()
{
  boost::sync_deque<int> q;

  boost::barrier go(2);

  boost::future<void> push_done;
  boost::future<int> pull_done;

  try
  {
    push_done=boost::async(boost::launch::async,
                           call_push_back<int>(&q,&go));
    pull_done=boost::async(boost::launch::async,
                           call_pull_front<int>(&q,&go));

    push_done.get();
    BOOST_TEST_EQ(pull_done.get(), 42);
    BOOST_TEST(q.empty());
  }
  catch (...)
  {
    BOOST_TEST(false);
  }
}

#if defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
void test_concurrent_push_back_and_wait_pull_front_on_empty_queue()
{
  boost::sync_deque<int> q;
  const unsigned int n = 3;
  boost::barrier go(n);

  boost::future<boost::queue_op_status> pull_done[n];
  int results[n];

  try
  {
    for (unsigned int i =0; i< n; ++i)
      pull_done[i]=boost::async(boost::launch::async,
                                call_wait_pull_front<int>(&q,&go),
                                boost::ref(results[i]));

    for (unsigned int i =0; i< n; ++i)
      q.push_back(42);

    for (unsigned int i = 0; i < n; ++i) {
      BOOST_TEST(pull_done[i].get() == boost::queue_op_status::success);
      BOOST_TEST_EQ(results[i], 42);
    }
    BOOST_TEST(q.empty());
  }
  catch (...)
  {
    BOOST_TEST(false);
  }
}

void test_concurrent_wait_pull_front_and_close_on_empty_queue()
{
  boost::sync_deque<int> q;
  const unsigned int n = 3;
  boost::barrier go(n);

  boost::future<boost::queue_op_status> pull_done[n];
  int results[n];

  try
  {
    for (unsigned int i =0; i< n; ++i)
      pull_done[i]=boost::async(boost::launch::async,
                                call_wait_pull_front<int>(&q,&go),
                                boost::ref(results[i]));

    q.close();

    for (unsigned int i = 0; i < n; ++i) {
      BOOST_TEST(pull_done[i].get() == boost::queue_op_status::closed);
    }
    BOOST_TEST(q.empty());
  }
  catch (...)
  {
    BOOST_TEST(false);
  }
}
#endif

void test_concurrent_push_back_on_empty_queue()
{
  boost::sync_deque<int> q;
  const unsigned int n = 3;
  boost::barrier go(n);
  boost::future<void> push_done[n];

  try
  {
    for (unsigned int i =0; i< n; ++i)
      push_done[i]=boost::async(boost::launch::async,
                                call_push_back<int>(&q,&go));

  }
  catch (...)
  {
    BOOST_TEST(false);
  }
  try
  {
    for (unsigned int i = 0; i < n; ++i)
      push_done[i].get();

  }
  catch (...)
  {
    BOOST_TEST(false);
  }
  try
  {
    BOOST_TEST(!q.empty());
    for (unsigned int i =0; i< n; ++i)
      BOOST_TEST_EQ(q.pull_front(), 42);
    BOOST_TEST(q.empty());

  }
  catch (...)
  {
    BOOST_TEST(false);
  }
}

void test_concurrent_pull_front_on_queue()
{
  boost::sync_deque<int> q;
  const unsigned int n = 3;
  boost::barrier go(n);

  boost::future<int> pull_done[n];

  try
  {
    for (unsigned int i =0; i< n; ++i)
      q.push_back(42);

    for (unsigned int i =0; i< n; ++i)
      pull_done[i]=boost::async(boost::launch::async,
#if ! defined BOOST_NO_CXX11_LAMBDAS
        [&q,&go]() -> int
        {
          go.wait();
          return q.pull_front();
        }
#else
        call_pull_front<int>(&q,&go)
#endif
      );

    for (unsigned int i = 0; i < n; ++i)
      BOOST_TEST_EQ(pull_done[i].get(), 42);
    BOOST_TEST(q.empty());
  }
  catch (...)
  {
    BOOST_TEST(false);
  }
}

int main()
{
  test_concurrent_push_back_and_pull_front_on_empty_queue();
#if defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
  test_concurrent_push_back_and_wait_pull_front_on_empty_queue();
  test_concurrent_wait_pull_front_and_close_on_empty_queue();
#endif
  test_concurrent_push_back_on_empty_queue();
  test_concurrent_pull_front_on_queue();
  return boost::report_errors();
}

