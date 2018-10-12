// Copyright (C) 2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/sync_queue.hpp>

// class sync_queue<T>

//    sync_queue();

#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_QUEUE_DEPRECATE_OLD

#include <boost/thread/concurrent_queues/sync_queue.hpp>
#include <boost/thread/concurrent_queues/queue_adaptor.hpp>
#include <boost/thread/concurrent_queues/queue_views.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/static_assert.hpp>

class non_copyable
{
  int val;
public:
  BOOST_THREAD_MOVABLE_ONLY(non_copyable)
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

#if defined  BOOST_NO_CXX11_RVALUE_REFERENCES
BOOST_STATIC_ASSERT( ! boost::is_copy_constructible<non_copyable>::value );
BOOST_STATIC_ASSERT( boost::has_move_emulation_enabled<non_copyable>::value );
#endif

int main()
{

  {
    // default queue invariants
      boost::queue_adaptor<boost::sync_queue<int> > sq;
      boost::queue_back<int> q(sq);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // default queue invariants
      boost::queue_adaptor<boost::sync_queue<int> > sq;
      boost::queue_front<int> q(sq);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }


  {
    // empty queue try_pull fails
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      int i;
      BOOST_TEST( boost::queue_op_status::empty == q.try_pull(i));
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push rvalue/copyable succeeds
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      q.push(1);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push lvalue/copyable succeeds
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      int i;
      q.push(i);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }


#if 0
  {
    // empty queue push rvalue/non_copyable succeeds
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_back<non_copyable> q(sq);
      q.push(non_copyable(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
#endif
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
  {
    // empty queue push rvalue/non_copyable succeeds
    boost::queue_adaptor<boost::sync_queue<non_copyable> > q;
    //boost::sync_queue<non_copyable> q;
    //boost::queue_back<non_copyable> q(sq);
      non_copyable nc(1);
      q.push(boost::move(nc));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
#endif

  {
    // empty queue push rvalue succeeds
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      q.push(1);
      q.push(2);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 2u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue push lvalue succeeds
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      int i;
      q.push(i);
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue/copyable succeeds
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      BOOST_TEST(boost::queue_op_status::success == q.try_push(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue/copyable succeeds
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      BOOST_TEST(boost::queue_op_status::success == q.try_push(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }

#if 0
  {
    // empty queue try_push rvalue/non-copyable succeeds
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_back<non_copyable> q(sq);
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      int i=0;
      BOOST_TEST(boost::queue_op_status::success == q.try_push(i));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
  {
    // empty queue try_push rvalue succeeds
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_push(1));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }


#if 0
  {
    // empty queue nonblocking_push rvalue/non-copyable succeeds
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_back<non_copyable> q(sq);
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_push(non_copyable(1)));
      BOOST_TEST(! q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 1u);
      BOOST_TEST(! q.closed());
  }
#endif
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
  {
    // empty queue nonblocking_push rvalue/non-copyable succeeds
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_back<non_copyable> q(sq);
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      sq.push(1);
      int i;
      q.pull(i);
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
  {
    // 1-element queue pull succeed
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_front<non_copyable> q(sq);
      non_copyable nc1(1);
      sq.push(boost::move(nc1));
      non_copyable nc2(2);
      q.pull(nc2);
      BOOST_TEST_EQ(nc1, nc2);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#endif
  {
    // 1-element queue pull succeed
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      sq.push(1);
      int i = q.pull();
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
  {
    // 1-element queue pull succeed
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_front<non_copyable> q(sq);
      non_copyable nc1(1);
      sq.push(boost::move(nc1));
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      sq.push(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.try_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
  {
    // 1-element queue try_pull succeed
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_front<non_copyable> q(sq);
      non_copyable nc1(1);
      sq.push(boost::move(nc1));
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      sq.push(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.nonblocking_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
  {
    // 1-element queue nonblocking_pull succeed
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_front<non_copyable> q(sq);
      non_copyable nc1(1);
      sq.push(boost::move(nc1));
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
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_front<non_copyable> q(sq);
      non_copyable nc1(1);
      sq.push(boost::move(nc1));
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      sq.push(1);
      int i;
      BOOST_TEST(boost::queue_op_status::success == q.wait_pull(i));
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(! q.closed());
  }
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1900)
  {
    // 1-element queue wait_pull succeed
    boost::queue_adaptor<boost::sync_queue<non_copyable> > sq;
    boost::queue_front<non_copyable> q(sq);
      non_copyable nc1(1);
      sq.push(boost::move(nc1));
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed invariants
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      q.close();
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // closed queue push fails
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_back<int> q(sq);
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      sq.push(1);
      q.close();
      int i;
      q.pull(i);
      BOOST_TEST_EQ(i, 1);
      BOOST_TEST(q.empty());
      BOOST_TEST(! q.full());
      BOOST_TEST_EQ(q.size(), 0u);
      BOOST_TEST(q.closed());
  }
  {
    // 1-element closed queue wait_pull succeed
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
      sq.push(1);
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
    boost::queue_adaptor<boost::sync_queue<int> > sq;
    boost::queue_front<int> q(sq);
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

