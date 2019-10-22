// Copyright (C) 2014 Ian Forbed
// Copyright (C) 2014,2015 Vicente J. Botet Escriba
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

#include <iostream>

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/concurrent_queues/sync_priority_queue.hpp>

#include <boost/detail/lightweight_test.hpp>
#include "../../../timming.hpp"

using namespace boost::chrono;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;

typedef boost::concurrent::sync_priority_queue<int> sync_pq;

class non_copyable
{
  BOOST_THREAD_MOVABLE_ONLY(non_copyable)
  int val;
public:
  non_copyable(int v) : val(v){}
  non_copyable(BOOST_RV_REF(non_copyable) x): val(x.val) {}
  non_copyable& operator=(BOOST_RV_REF(non_copyable) x) { val=x.val; return *this; }
  bool operator==(non_copyable const& x) const {return val==x.val;}
  template <typename OSTREAM>
  friend OSTREAM& operator <<(OSTREAM& os, non_copyable const&x )
  {
    os << x.val;
    return os;
  }
  bool operator <(const non_copyable& other) const
  {
    return val < other.val;
  }
};

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void test_pull_for()
{
  sync_pq pq;
  steady_clock::time_point start = steady_clock::now();
  int val;
  boost::queue_op_status st = pq.pull_for(milliseconds(500), val);
  ns d = steady_clock::now() - start - milliseconds(500);
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
  BOOST_TEST(boost::queue_op_status::timeout == st);
}

void test_pull_until()
{
  sync_pq pq;
  steady_clock::time_point start = steady_clock::now();
  int val;
  boost::queue_op_status st = pq.pull_until(start + milliseconds(500), val);
  ns d = steady_clock::now() - start - milliseconds(500);
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
  BOOST_TEST(boost::queue_op_status::timeout == st);
}

void test_nonblocking_pull()
{
  sync_pq pq;
  steady_clock::time_point start = steady_clock::now();
  int val;
  boost::queue_op_status st = pq.nonblocking_pull(val);
  ns d = steady_clock::now() - start;
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
  BOOST_TEST(boost::queue_op_status::empty == st);
}

void test_pull_for_when_not_empty()
{
  sync_pq pq;
  pq.push(1);
  steady_clock::time_point start = steady_clock::now();
  int val;
  boost::queue_op_status st = pq.pull_for(milliseconds(500), val);
  ns d = steady_clock::now() - start;
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
  BOOST_TEST(boost::queue_op_status::success == st);
  BOOST_TEST(1 == val);
}

void test_pull_until_when_not_empty()
{
  sync_pq pq;
  pq.push(1);
  steady_clock::time_point start = steady_clock::now();
  int val;
  boost::queue_op_status st = pq.pull_until(start + milliseconds(500), val);
  ns d = steady_clock::now() - start;
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
  BOOST_TEST(boost::queue_op_status::success == st);
  BOOST_TEST(1 == val);
}

int main()
{
  sync_pq pq;
  BOOST_TEST(pq.empty());
  BOOST_TEST(!pq.closed());
  BOOST_TEST_EQ(pq.size(), std::size_t(0));

  for(int i = 1; i <= 5; i++){
    pq.push(i);
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(i));
  }

  for(int i = 6; i <= 10; i++){
    boost::queue_op_status succ = pq.try_push(i);
    BOOST_TEST(succ == boost::queue_op_status::success );
    BOOST_TEST(!pq.empty());
    BOOST_TEST_EQ(pq.size(), std::size_t(i));
  }

  for(int i = 10; i > 5; i--){
    int val = pq.pull();
    BOOST_TEST_EQ(val, i);
  }

//  for(int i = 5; i > 0; i--){
//    boost::optional<int> val = pq.try_pull();
//    BOOST_TEST(val);
//    BOOST_TEST_EQ(*val, i);
//  }

//  BOOST_TEST(pq.empty());
  pq.close();
  BOOST_TEST(pq.closed());

  test_pull_for();
  test_pull_until();
  test_nonblocking_pull();

  test_pull_for_when_not_empty();
  //test_pull_until_when_not_empty();

#if ! defined  BOOST_NO_CXX11_RVALUE_REFERENCES
  {
    // empty queue try_push rvalue/non-copyable succeeds
      boost::concurrent::sync_priority_queue<non_copyable> q;
      BOOST_TEST(boost::queue_op_status::success ==q.try_push(non_copyable(1)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    //fixme
    // empty queue try_push rvalue/non-copyable succeeds
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc(1);
      BOOST_TEST(boost::queue_op_status::success == q.try_push(boost::move(nc)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
#endif

  {
    // empty queue try_push lvalue succeeds
      boost::concurrent::sync_priority_queue<int> q;
      int i=1;
      BOOST_TEST(boost::queue_op_status::success == q.try_push(i));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
#if 0
  {
    // empty queue try_push rvalue succeeds
      boost::concurrent::sync_priority_queue<int> q;
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_push(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue nonblocking_push rvalue/non-copyable succeeds
      boost::concurrent::sync_priority_queue<non_copyable> q;
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_push(non_copyable(1)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue nonblocking_push rvalue/non-copyable succeeds
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc(1);
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_push(boost::move(nc)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
#endif

  {
    // 1-element queue pull succeed
      boost::concurrent::sync_priority_queue<int> q;
      q.push(1);
      int i;
      i=q.pull();
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if ! defined  BOOST_NO_CXX11_RVALUE_REFERENCES
  {
    // 1-element queue pull succeed
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc1(1);
      q.push(boost::move(nc1));
      non_copyable nc2(2);
      nc2=q.pull();
      BOOST_TEST_EQ(nc1, nc2);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#endif

  {
    // 1-element queue pull succeed
      boost::concurrent::sync_priority_queue<int> q;
      q.push(1);
      int i = q.pull();
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if ! defined  BOOST_NO_CXX11_RVALUE_REFERENCES
  {
    // 1-element queue pull succeed
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc1(1);
      q.push(boost::move(nc1));
      non_copyable nc = q.pull();
      BOOST_TEST_EQ(nc, nc1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#endif

  {
    // 1-element queue try_pull succeed
      boost::concurrent::sync_priority_queue<int> q;
      q.push(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.try_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if ! defined  BOOST_NO_CXX11_RVALUE_REFERENCES
  {
    // 1-element queue try_pull succeed
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc1(1);
      q.push(boost::move(nc1));
      non_copyable nc(2);
      BOOST_TEST(boost::queue_op_status::success == q.try_pull(nc));
      BOOST_TEST_EQ(nc, nc1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#endif
  {
    // 1-element queue nonblocking_pull succeed
      boost::concurrent::sync_priority_queue<int> q;
      q.push(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if ! defined  BOOST_NO_CXX11_RVALUE_REFERENCES
  {
    // 1-element queue nonblocking_pull succeed
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc1(1);
      q.push(boost::move(nc1));
      non_copyable nc(2);
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_pull(nc));
      BOOST_TEST_EQ(nc, nc1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue wait_pull succeed
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc1(1);
      q.push(boost::move(nc1));
      non_copyable nc(2);
      BOOST_TEST(boost::queue_op_status::success == q.wait_pull(nc));
      BOOST_TEST_EQ(nc, nc1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#endif
  {
    // 1-element queue wait_pull succeed
      boost::concurrent::sync_priority_queue<int> q;
      q.push(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.wait_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if ! defined  BOOST_NO_CXX11_RVALUE_REFERENCES
  {
    // 1-element queue wait_pull succeed
      boost::concurrent::sync_priority_queue<non_copyable> q;
      non_copyable nc1(1);
      q.push(boost::move(nc1));
      non_copyable nc(2);
      BOOST_TEST(boost::queue_op_status::success == q.wait_pull(nc));
      BOOST_TEST_EQ(nc, nc1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#endif

  {
    // closed invariants
      boost::concurrent::sync_priority_queue<int> q;
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed queue push fails
      boost::concurrent::sync_priority_queue<int> q;
      q.close();
      try {
        q.push(1);
        BOOST_TEST(false); // fixme
      } catch (...) {
        BOOST_TEST(q.empty());
        BOOST_TEST(! q.full());
        BOOST_TEST_EQ(q.size(), 0u);
        BOOST_TEST(q.closed());
      }
  }
  {
    // 1-element closed queue pull succeed
      boost::concurrent::sync_priority_queue<int> q;
      q.push(1);
      q.close();
      int i;
      i=q.pull();
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // 1-element closed queue wait_pull succeed
      boost::concurrent::sync_priority_queue<int> q;
      q.push(1);
      q.close();
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.wait_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed empty queue wait_pull fails
      boost::concurrent::sync_priority_queue<int> q;
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(q.closed());
      int i;
      BOOST_TEST(boost::queue_op_status::closed == q.wait_pull(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(q.closed());
  }
  return boost::report_errors();
}
