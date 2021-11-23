// thread_safe_signals library
// Some assorted tests to expose various bugs that existed at some point,
// to make sure they stay fixed

// Copyright Frank Mori Hess 2008
// Use, modification and
// distribution is subject to the Boost Software License, Version
// 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/signals2.hpp>
#define BOOST_TEST_MODULE regression_test
#include <boost/test/included/unit_test.hpp>

typedef boost::signals2::signal<void ()> sig0_type;

// combiner that returns the number of slots invoked
struct slot_counter {
  typedef unsigned result_type;
  template<typename InputIterator>
  unsigned operator()(InputIterator first, InputIterator last) const
  {
    unsigned count = 0;
    for (; first != last; ++first)
    {
      try
      {
        *first;
        ++count;
      }
      catch(const boost::bad_weak_ptr &)
      {}
    }
    return count;
  }
};

void my_slot()
{
}

void my_connecting_slot(sig0_type &sig)
{
  sig.connect(&my_slot);
}

void slot_connect_test()
{
  sig0_type sig;
  sig.connect(sig0_type::slot_type(&my_connecting_slot, boost::ref(sig)).track(sig));
  /* 2008-02-28: the following signal invocation triggered a (bogus) failed assertion of _shared_state.unique()
  at detail/signal_template.hpp:285 */
  sig();
  BOOST_CHECK(sig.num_slots() == 2);
  sig.disconnect(&my_slot);
  BOOST_CHECK(sig.num_slots() == 1);
  /* 2008-03-11: checked iterator barfed on next line, due to bad semantics of copy construction
  for boost::signals2::detail::grouped_list */
  sig();
  BOOST_CHECK(sig.num_slots() == 2);
}

/* 2008-03-10: we weren't disconnecting old connection in scoped_connection assignment operator */
void scoped_connection_test()
{
  typedef boost::signals2::signal<void (), slot_counter> signal_type;
  signal_type sig;
  {
    boost::signals2::scoped_connection conn(sig.connect(&my_slot));
    BOOST_CHECK(sig() == 1);
    conn = sig.connect(&my_slot);
    BOOST_CHECK(sig() == 1);
  }
  BOOST_CHECK(sig() == 0);
}

// testsignal that returns a reference type

struct ref_returner
{
  static int i;
  
  int& ref_return_slot()
  {
    return i;
  }
};

int ref_returner::i = 0;

void reference_return_test()
{
  boost::signals2::signal<int& ()> rTest;
  ref_returner rr;
  rTest.connect(boost::bind(&ref_returner::ref_return_slot, &rr));
  int& r = *rTest();
  BOOST_CHECK(ref_returner::i == 0);
  r = 1;
  BOOST_CHECK(ref_returner::i == 1);
}

BOOST_AUTO_TEST_CASE(test_main)
{
  slot_connect_test();
  scoped_connection_test();
  reference_return_test();
}
