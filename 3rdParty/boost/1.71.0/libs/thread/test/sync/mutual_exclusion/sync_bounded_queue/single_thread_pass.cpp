// Copyright (C) 2013 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/sync_bounded_queue.hpp>

// class sync_bounded_queue<T>

//    sync_bounded_queue();

#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_QUEUE_DEPRECATE_OLD

#include <boost/thread/sync_bounded_queue.hpp>

#include <boost/detail/lightweight_test.hpp>

class non_copyable
{
  BOOST_THREAD_MOVABLE_ONLY(non_copyable)
  int val;
public:
  non_copyable() {}
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

};


int main()
{

  {
    // default queue invariants
      boost::sync_bounded_queue<int> q(2);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST_EQ(q.capacity(), 2u);
      BOOST_TEST(! q.closed());
  }
#ifndef BOOST_THREAD_QUEUE_DEPRECATE_OLD
  {
    // empty queue try_pull fails
      boost::sync_bounded_queue<int> q(2);
      int i;
      BOOST_TEST(! q.try_pull(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_pull fails
      boost::sync_bounded_queue<int> q(2);
      BOOST_TEST(! q.try_pull());
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push rvalue succeeds
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push rvalue succeeds
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      q.push(boost::move(nc));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push rvalue succeeds
      boost::sync_bounded_queue<int> q(3);
      q.push(1);
      BOOST_TEST_EQ(q.size(), 1u);
      q.push(2);
      BOOST_TEST_EQ(q.size(), 2u);
      q.push(2);
      BOOST_TEST_EQ(q.size(), 3u);
      BOOST_TEST(! q.empty());
      BOOST_TEST( q.full());
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push value succeeds
      boost::sync_bounded_queue<int> q(2);
      int i;
      q.push(i);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<int> q(2);
      BOOST_TEST(q.try_push(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      BOOST_TEST(q.try_push(boost::move(nc)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push value succeeds
      boost::sync_bounded_queue<int> q(2);
      int i=1;
      BOOST_TEST(q.try_push(i));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<int> q(2);
      BOOST_TEST(q.try_push(boost::no_block, 1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      BOOST_TEST(q.try_push(boost::no_block, boost::move(nc)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      int i;
      q.pull(i);
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue pull succeed
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      q.push(boost::move(nc));
      non_copyable nc2(2);
      q.pull(nc2);
      BOOST_TEST_EQ(nc, nc2);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      int i = q.pull();
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue try_pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      int i;
      BOOST_TEST(q.try_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue try_pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      int i;
      BOOST_TEST(q.try_pull(boost::no_block, i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue try_pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      boost::shared_ptr<int> i = q.try_pull();
      BOOST_TEST_EQ(*i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // full queue try_push rvalue fails
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      q.push(2);
      BOOST_TEST(! q.try_push(3));
      BOOST_TEST(! q.empty());
      BOOST_TEST( q.full());
      BOOST_TEST_EQ(q.size(), 2u);
      BOOST_TEST(! q.closed());
  }
  {
    // full queue try_push succeeds
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      q.push(2);
      BOOST_TEST(q.try_pull());
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }

  {
    // closed invariants
      boost::sync_bounded_queue<int> q(2);
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed queue push fails
      boost::sync_bounded_queue<int> q(2);
      q.close();
      try {
        q.push(1);
        BOOST_TEST(false);
      } catch (...) {
        BOOST_TEST(q.empty());
        BOOST_TEST(! q.full());
        BOOST_TEST_EQ(q.size(), 0u);
        BOOST_TEST(q.closed());
      }
  }
  {
    // 1-element closed queue pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push(1);
      q.close();
      int i;
      q.pull(i);
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
#endif
  {
    // empty queue try_pull fails
      boost::sync_bounded_queue<int> q(2);
      int i;
      BOOST_TEST( boost::queue_op_status::empty == q.try_pull_front(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push rvalue succeeds
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push rvalue succeeds
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      q.push_back(boost::move(nc));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push rvalue succeeds
      boost::sync_bounded_queue<int> q(3);
      q.push_back(1);
      BOOST_TEST_EQ(q.size(), 1u);
      q.push_back(2);
      BOOST_TEST_EQ(q.size(), 2u);
      q.push_back(3);
      BOOST_TEST_EQ(q.size(), 3u);
      BOOST_TEST(! q.empty());
      BOOST_TEST( q.full());
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push value succeeds
      boost::sync_bounded_queue<int> q(2);
      int i;
      q.push_back(i);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<int> q(2);
      BOOST_TEST(boost::queue_op_status::success == q.try_push_back(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      BOOST_TEST(boost::queue_op_status::success == q.try_push_back(boost::move(nc)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push value succeeds
      boost::sync_bounded_queue<int> q(2);
      int i=1;
      BOOST_TEST(boost::queue_op_status::success == q.try_push_back(i));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<int> q(2);
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_push_back( 1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_push_back(boost::move(nc)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue wait_push rvalue succeeds
      boost::sync_bounded_queue<int> q(2);
      BOOST_TEST(boost::queue_op_status::success == q.wait_push_back(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue wait_push rvalue succeeds
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      BOOST_TEST(boost::queue_op_status::success == q.wait_push_back(boost::move(nc)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue wait_push value succeeds
      boost::sync_bounded_queue<int> q(2);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.wait_push_back(i));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      int i;
      q.pull_front(i);
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue pull succeed
      boost::sync_bounded_queue<non_copyable> q(2);
      non_copyable nc(1);
      q.push_back(boost::move(nc));
      non_copyable nc2(2);
      q.pull_front(nc2);
      BOOST_TEST_EQ(nc, nc2);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      int i = q.pull_front();
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue try_pull succeed
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.try_pull_front(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue nonblocking_pull_front succeed
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_pull_front(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // 1-element queue wait_pull_front succeed
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.wait_pull_front(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // full queue try_push rvalue fails
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      q.push_back(2);
      BOOST_TEST(boost::queue_op_status::full == q.try_push_back(3));
      BOOST_TEST(! q.empty());
      BOOST_TEST( q.full());
      BOOST_TEST_EQ(q.size(), 2u);
      BOOST_TEST(! q.closed());
  }

  {
    // closed invariants
      boost::sync_bounded_queue<int> q(2);
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed queue push fails
      boost::sync_bounded_queue<int> q(2);
      q.close();
      try {
        q.push_back(1);
        BOOST_TEST(false);
      } catch (...) {
        BOOST_TEST(q.empty());
        BOOST_TEST(! q.full());
        BOOST_TEST_EQ(q.size(), 0u);
        BOOST_TEST(q.closed());
      }
  }
  {
    // closed empty queue try_pull_front closed
      boost::sync_bounded_queue<int> q(2);
      q.close();
      int i;
      BOOST_TEST(boost::queue_op_status::closed == q.try_pull_front(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed empty queue nonblocking_pull_front closed
      boost::sync_bounded_queue<int> q(2);
      q.close();
      int i;
      BOOST_TEST(boost::queue_op_status::closed == q.nonblocking_pull_front(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // 1-element closed queue pull_front succeed
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      q.close();
      int i;
      q.pull_front(i);
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // 1-element closed queue wait_pull_front succeed
      boost::sync_bounded_queue<int> q(2);
      q.push_back(1);
      q.close();
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.wait_pull_front(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed empty queue wait_pull_front closed
      boost::sync_bounded_queue<int> q(2);
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(q.closed());
      int i;
      BOOST_TEST(boost::queue_op_status::closed == q.wait_pull_front(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(q.closed());
  }
  {
    // closed queue wait_push_back fails
      boost::sync_bounded_queue<int> q(2);
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(q.closed());
      int i;
      BOOST_TEST(boost::queue_op_status::closed == q.wait_push_back(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(q.closed());
  }
  return boost::report_errors();
}

