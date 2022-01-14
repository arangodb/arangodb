
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// This test is based on the tests of Boost.Thread

#include <sstream>
#include <string>

#include <boost/test/unit_test.hpp>

#include <boost/fiber/all.hpp>

int value1 = 0;
int value2 = 0;

void fn1( boost::fibers::barrier & b) {
    ++value1;
    boost::this_fiber::yield();

    b.wait();

    ++value1;
    boost::this_fiber::yield();
    ++value1;
    boost::this_fiber::yield();
    ++value1;
    boost::this_fiber::yield();
    ++value1;
}

void fn2( boost::fibers::barrier & b) {
    ++value2;
    boost::this_fiber::yield();
    ++value2;
    boost::this_fiber::yield();
    ++value2;
    boost::this_fiber::yield();

    b.wait();

    ++value2;
    boost::this_fiber::yield();
    ++value2;
}

void test_barrier() {
    value1 = 0;
    value2 = 0;

    boost::fibers::barrier b( 2);
    boost::fibers::fiber f1( boost::fibers::launch::post, fn1, std::ref( b) );
    boost::fibers::fiber f2( boost::fibers::launch::post, fn2, std::ref( b) );

    f1.join();
    f2.join();

    BOOST_CHECK_EQUAL( 5, value1);
    BOOST_CHECK_EQUAL( 5, value2);
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* []) {
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Fiber: barrier test suite");

    test->add( BOOST_TEST_CASE( & test_barrier) );

    return test;
}
