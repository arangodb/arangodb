//  (C) Copyright Gennadiy Rozental 2008-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : tests Unit Test Framework usability in MT environment with
//                Boost.Test calls externally synchronized
// ***************************************************************************

#define BOOST_TEST_MODULE sync_access_test
#include <boost/test/unit_test.hpp>

#include <boost/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>

using namespace boost;
namespace ut = boost::unit_test;

static boost::mutex m;

/// thread execution function
static void thread_function(boost::barrier& b)
{
    b.wait(); /// wait until memory barrier allows the execution
    boost::mutex::scoped_lock lock(m); /// lock mutex
    BOOST_TEST(1 ==0 ); /// produce the fault
}

#if BOOST_PP_VARIADICS
/// test function which creates threads
BOOST_AUTO_TEST_CASE( test_multiple_assertion_faults, * ut::expected_failures(100))
#else
BOOST_TEST_DECORATOR(
  * ut::expected_failures(100)
)
BOOST_AUTO_TEST_CASE( test_multiple_assertion_faults)
#endif
{
    boost::thread_group tg;      // thread group to manage all threads
    boost::barrier      b(100);  // memory barrier, which should block all threads
                                 // until all 100 threads were created

    for(size_t i=0; i<100; ++i)
        tg.create_thread(boost::bind(thread_function, ref(b))); /// create a thread and pass it the barrier

    tg.join_all();
}

// EOF
