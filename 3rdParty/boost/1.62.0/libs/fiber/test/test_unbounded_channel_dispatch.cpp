
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <chrono>
#include <sstream>
#include <string>

#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/fiber/all.hpp>

struct moveable {
    bool    state;
    int     value;

    moveable() :
        state( false),
        value( -1) {
    }

    moveable( int v) :
        state( true),
        value( v) {
    }

    moveable( moveable && other) :
        state( other.state),
        value( other.value) {
        other.state = false;
        other.value = -1;
    }

    moveable & operator=( moveable && other) {
        if ( this == & other) return * this;
        state = other.state;
        other.state = false;
        value = other.value;
        other.value = -1;
        return * this;
    }
};

void test_push() {
    boost::fibers::unbounded_channel< int > c;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 1) );
}

void test_push_closed() {
    boost::fibers::unbounded_channel< int > c;
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.push( 1) );
}

void test_pop() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_closed() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop( v2) );
}

void test_pop_success() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_value_pop() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    v2 = c.value_pop();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_value_pop_closed() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    v2 = c.value_pop();
    BOOST_CHECK_EQUAL( v1, v2);
    bool thrown = false;
    try {
        c.value_pop();
    } catch ( boost::fibers::fiber_error const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_value_pop_success() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&c,&v2](){
        v2 = c.value_pop();
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_try_pop() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.try_pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_try_pop_closed() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.try_pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.try_pop( v2) );
}

void test_try_pop_success() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&c,&v2](){
        while ( boost::fibers::channel_op_status::success != c.try_pop( v2) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for_closed() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
}

void test_pop_wait_for_success() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for_timeout() {
    boost::fibers::unbounded_channel< int > c;
    int v = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v](){
        BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.pop_wait_for( v, std::chrono::seconds( 1) ) );
    });
    f.join();
}

void test_pop_wait_until() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
            std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_until_closed() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
            std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop_wait_until( v2,
            std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
}

void test_pop_wait_until_success() {
    boost::fibers::unbounded_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_until_timeout() {
    boost::fibers::unbounded_channel< int > c;
    int v = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v](){
        BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.pop_wait_until( v,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    });
    f.join();
}

void test_moveable() {
    boost::fibers::unbounded_channel< moveable > c;
    moveable m1( 3), m2;
    BOOST_CHECK( m1.state);
    BOOST_CHECK_EQUAL( 3, m1.value);
    BOOST_CHECK( ! m2.state);
    BOOST_CHECK_EQUAL( -1, m2.value);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( std::move( m1) ) );
    BOOST_CHECK( ! m1.state);
    BOOST_CHECK( ! m2.state);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( m2) );
    BOOST_CHECK( ! m1.state);
    BOOST_CHECK_EQUAL( -1, m1.value);
    BOOST_CHECK( m2.state);
    BOOST_CHECK_EQUAL( 3, m2.value);
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* []) {
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Fiber: unbounded_channel test suite");

     test->add( BOOST_TEST_CASE( & test_push) );
     test->add( BOOST_TEST_CASE( & test_push_closed) );
     test->add( BOOST_TEST_CASE( & test_pop) );
     test->add( BOOST_TEST_CASE( & test_pop_closed) );
     test->add( BOOST_TEST_CASE( & test_pop_success) );
     test->add( BOOST_TEST_CASE( & test_value_pop) );
     test->add( BOOST_TEST_CASE( & test_value_pop_closed) );
     test->add( BOOST_TEST_CASE( & test_value_pop_success) );
     test->add( BOOST_TEST_CASE( & test_try_pop) );
     test->add( BOOST_TEST_CASE( & test_try_pop_closed) );
     test->add( BOOST_TEST_CASE( & test_try_pop_success) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for_closed) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for_success) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for_timeout) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until_closed) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until_success) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until_timeout) );
     test->add( BOOST_TEST_CASE( & test_moveable) );

    return test;
}
