
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

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
    boost::fibers::unbuffered_channel< int > c;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 1) );
    });
    int value = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( value) );
    BOOST_CHECK_EQUAL( 1, value);
    f.join();
}

void test_push_closed() {
    boost::fibers::unbuffered_channel< int > c;
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.push( 1) );
}


void test_push_wait_for() {
    boost::fibers::unbuffered_channel< int > c;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
    });
    int value = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( value) );
    BOOST_CHECK_EQUAL( 1, value);
    f.join();
}

void test_push_wait_for_closed() {
    boost::fibers::unbuffered_channel< int > c;
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
}

void test_push_wait_for_timeout() {
    boost::fibers::unbuffered_channel< int > c;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c](){
        int value = 0;
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( value) );
        BOOST_CHECK_EQUAL( 1, value);
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
    BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
    f.join();
}

void test_push_wait_until() {
    boost::fibers::unbuffered_channel< int > c;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_until( 1,
                        std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    });
    int value = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( value) );
    BOOST_CHECK_EQUAL( 1, value);
    f.join();
}

void test_push_wait_until_closed() {
    boost::fibers::unbuffered_channel< int > c;
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.push_wait_until( 1,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
}

void test_push_wait_until_timeout() {
    boost::fibers::unbuffered_channel< int > c;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c](){
        int value = 0;
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( value) );
        BOOST_CHECK_EQUAL( 1, value);
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_until( 1,
                std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.push_wait_until( 1,
                std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    f.join();
}

void test_pop() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&v1,&c](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
    f.join();
}

void test_pop_closed() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&v1,&c](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
        c.close();
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop( v2) );
    f.join();
}

void test_pop_success() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    f.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_value_pop() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    v2 = c.value_pop();
    f.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_value_pop_closed() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
        c.close();
    });
    int v2 = c.value_pop();
    BOOST_CHECK_EQUAL( v1, v2);
    f.join();
    bool thrown = false;
    try {
        c.value_pop();
    } catch ( boost::fibers::fiber_error const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_value_pop_success() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v2](){
        v2 = c.value_pop();
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    f.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    f.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for_closed() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
        c.close();
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    f.join();
}

void test_pop_wait_for_success() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    f.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for_timeout() {
    boost::fibers::unbuffered_channel< int > c;
    int v = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v](){
        BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.pop_wait_for( v, std::chrono::seconds( 1) ) );
    });
    f.join();
}

void test_pop_wait_until() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
            std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
    f.join();
}

void test_pop_wait_until_closed() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
        c.close();
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
            std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop_wait_until( v2,
            std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    f.join();
}

void test_pop_wait_until_success() {
    boost::fibers::unbuffered_channel< int > c;
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    });
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    f.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_until_timeout() {
    boost::fibers::unbuffered_channel< int > c;
    int v = 0;
    BOOST_CHECK(
            boost::fibers::channel_op_status::timeout == c.pop_wait_until( v,
                std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
}

void test_wm_1() {
    boost::fibers::unbuffered_channel< int > c;
    std::vector< boost::fibers::fiber::id > ids;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&c,&ids](){
        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 1) );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 2) );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 3) );

        ids.push_back( boost::this_fiber::get_id() );
        // would be blocked because channel is full
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 4) );

        ids.push_back( boost::this_fiber::get_id() );
        // would be blocked because channel is full
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 5) );

        ids.push_back( boost::this_fiber::get_id() );
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&c,&ids](){
        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 1, c.value_pop() );

        // let other fiber run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 2, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 3, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 4, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
        // would block because channel is empty
        BOOST_CHECK_EQUAL( 5, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
    });
    boost::fibers::fiber::id id1 = f1.get_id();
    boost::fibers::fiber::id id2 = f2.get_id();
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( 12u, ids.size() );
    BOOST_CHECK_EQUAL( id1, ids[0]);
    BOOST_CHECK_EQUAL( id2, ids[1]);
    BOOST_CHECK_EQUAL( id1, ids[2]);
    BOOST_CHECK_EQUAL( id2, ids[3]);
    BOOST_CHECK_EQUAL( id2, ids[4]);
    BOOST_CHECK_EQUAL( id1, ids[5]);
    BOOST_CHECK_EQUAL( id2, ids[6]);
    BOOST_CHECK_EQUAL( id1, ids[7]);
    BOOST_CHECK_EQUAL( id2, ids[8]);
    BOOST_CHECK_EQUAL( id1, ids[9]);
    BOOST_CHECK_EQUAL( id2, ids[10]);
    BOOST_CHECK_EQUAL( id1, ids[11]);
}

void test_moveable() {
    boost::fibers::unbuffered_channel< moveable > c;
    boost::fibers::fiber f( boost::fibers::launch::dispatch, [&c]{
        moveable m1( 3);
        BOOST_CHECK( m1.state);
        BOOST_CHECK_EQUAL( 3, m1.value);
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( std::move( m1) ) );
    });
    moveable m2;
    BOOST_CHECK( ! m2.state);
    BOOST_CHECK_EQUAL( -1, m2.value);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( m2) );
    BOOST_CHECK( m2.state);
    BOOST_CHECK_EQUAL( 3, m2.value);
    f.join();
}

void test_rangefor() {
    boost::fibers::unbuffered_channel< int > chan;
    std::vector< int > vec;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&chan]{
        chan.push( 1);
        chan.push( 1);
        chan.push( 2);
        chan.push( 3);
        chan.push( 5);
        chan.push( 8);
        chan.push( 12);
        chan.close();
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&vec,&chan]{
        for ( int value : chan) {
            vec.push_back( value);
        }
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( 1, vec[0]);
    BOOST_CHECK_EQUAL( 1, vec[1]);
    BOOST_CHECK_EQUAL( 2, vec[2]);
    BOOST_CHECK_EQUAL( 3, vec[3]);
    BOOST_CHECK_EQUAL( 5, vec[4]);
    BOOST_CHECK_EQUAL( 8, vec[5]);
    BOOST_CHECK_EQUAL( 12, vec[6]);
}

void test_issue_181() {
    boost::fibers::unbuffered_channel< int > chan;
    boost::fibers::fiber f1( boost::fibers::launch::dispatch, [&chan]() {
        auto state = chan.push( 1);
        BOOST_CHECK( boost::fibers::channel_op_status::closed == state);
    });
    boost::fibers::fiber f2( boost::fibers::launch::dispatch, [&chan]() {
        boost::this_fiber::sleep_for( std::chrono::milliseconds( 100) );
        chan.close();
    });
    f2.join();
    f1.join();
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* []) {
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Fiber: unbuffered_channel test suite");

     test->add( BOOST_TEST_CASE( & test_push) );
     test->add( BOOST_TEST_CASE( & test_push_closed) );
     test->add( BOOST_TEST_CASE( & test_push_wait_for) );
     test->add( BOOST_TEST_CASE( & test_push_wait_for_closed) );
     test->add( BOOST_TEST_CASE( & test_push_wait_for_timeout) );
     test->add( BOOST_TEST_CASE( & test_push_wait_until) );
     test->add( BOOST_TEST_CASE( & test_push_wait_until_closed) );
     test->add( BOOST_TEST_CASE( & test_push_wait_until_timeout) );
     test->add( BOOST_TEST_CASE( & test_pop) );
     test->add( BOOST_TEST_CASE( & test_pop_closed) );
     test->add( BOOST_TEST_CASE( & test_pop_success) );
     test->add( BOOST_TEST_CASE( & test_value_pop) );
     test->add( BOOST_TEST_CASE( & test_value_pop_closed) );
     test->add( BOOST_TEST_CASE( & test_value_pop_success) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for_closed) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for_success) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_for_timeout) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until_closed) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until_success) );
     test->add( BOOST_TEST_CASE( & test_pop_wait_until_timeout) );
     test->add( BOOST_TEST_CASE( & test_wm_1) );
     test->add( BOOST_TEST_CASE( & test_moveable) );
     test->add( BOOST_TEST_CASE( & test_rangefor) );
     test->add( BOOST_TEST_CASE( & test_issue_181) );

    return test;
}
