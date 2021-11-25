//  Copyright (C) 2011 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#define BOOST_TEST_MAIN
#ifdef BOOST_LOCKFREE_INCLUDE_TESTS
#include <boost/test/included/unit_test.hpp>
#else
#include <boost/test/unit_test.hpp>
#endif


int g_instance_counter = 0;

struct tester
{
    tester()
    {
        ++g_instance_counter;
    }

    tester(tester const&)
    {
        ++g_instance_counter;
    }

    ~tester()
    {
        --g_instance_counter;
    }
};

BOOST_AUTO_TEST_CASE( stack_instance_deleter_test )
{
    {
        boost::lockfree::stack<tester> q(128);
        q.push(tester());
        q.push(tester());
        q.push(tester());
        q.push(tester());
        q.push(tester());
    }

    assert(g_instance_counter == 0);
    BOOST_REQUIRE(g_instance_counter == 0);
}


BOOST_AUTO_TEST_CASE( spsc_queue_instance_deleter_test )
{
    {
        boost::lockfree::spsc_queue<tester> q(128);
        q.push(tester());
        q.push(tester());
        q.push(tester());
        q.push(tester());
        q.push(tester());
    }

    assert(g_instance_counter == 0);
    BOOST_REQUIRE(g_instance_counter == 0);
}

BOOST_AUTO_TEST_CASE( spsc_queue_fixed_sized_instance_deleter_test )
{
    {
        boost::lockfree::spsc_queue<tester, boost::lockfree::capacity<128> > q;
        q.push(tester());
        q.push(tester());
        q.push(tester());
        q.push(tester());
        q.push(tester());
    }

    assert(g_instance_counter == 0);
    BOOST_REQUIRE(g_instance_counter == 0);
}
