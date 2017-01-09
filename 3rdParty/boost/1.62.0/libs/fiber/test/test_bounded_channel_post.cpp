
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

void test_zero_wm_1() {
    bool thrown = false;
    try {
        boost::fibers::bounded_channel< int > c( 0);
    } catch ( boost::fibers::fiber_error const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_zero_wm_2() {
    bool thrown = false;
    try {
        boost::fibers::bounded_channel< int > c( 0, 0);
    } catch ( boost::fibers::fiber_error const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_hwm_less_lwm() {
    bool thrown = false;
    try {
        boost::fibers::bounded_channel< int > c( 2, 3);
    } catch ( boost::fibers::fiber_error const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_hwm_equal_lwm() {
    bool thrown = false;
    try {
        boost::fibers::bounded_channel< int > c( 3, 3);
    } catch ( boost::fibers::fiber_error const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_push() {
    boost::fibers::bounded_channel< int > c( 10);
    BOOST_CHECK_EQUAL( c.upper_bound(), 10u );
    BOOST_CHECK_EQUAL( c.lower_bound(), 9u );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 1) );
}

void test_push_closed() {
    boost::fibers::bounded_channel< int > c( 10);
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.push( 1) );
}

void test_try_push() {
    boost::fibers::bounded_channel< int > c( 1);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 1) );
}

void test_try_push_closed() {
    boost::fibers::bounded_channel< int > c( 1);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.try_push( 1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.try_push( 2) );
}

void test_try_push_full() {
    boost::fibers::bounded_channel< int > c( 1);
    BOOST_CHECK_EQUAL( c.lower_bound(), 0u );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.try_push( 1) );
    BOOST_CHECK( boost::fibers::channel_op_status::full == c.try_push( 2) );
}

void test_push_wait_for() {
    boost::fibers::bounded_channel< int > c( 2);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
}

void test_push_wait_for_closed() {
    boost::fibers::bounded_channel< int > c( 1);
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
}

void test_push_wait_for_timeout() {
    boost::fibers::bounded_channel< int > c( 1);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
    BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.push_wait_for( 1, std::chrono::seconds( 1) ) );
}

void test_push_wait_until() {
    boost::fibers::bounded_channel< int > c( 2);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_until( 1,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
}

void test_push_wait_until_closed() {
    boost::fibers::bounded_channel< int > c( 1);
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.push_wait_until( 1,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
}

void test_push_wait_until_timeout() {
    boost::fibers::bounded_channel< int > c( 1);
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push_wait_until( 1,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.push_wait_until( 1,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
}

void test_pop() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_closed() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop( v2) );
}

void test_pop_success() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop( v2) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_value_pop() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    v2 = c.value_pop();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_value_pop_closed() {
    boost::fibers::bounded_channel< int > c( 10);
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
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&v2](){
        v2 = c.value_pop();
    });
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_try_pop() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.try_pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_try_pop_closed() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.try_pop( v2) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.try_pop( v2) );
}

void test_try_pop_success() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&v2](){
        while ( boost::fibers::channel_op_status::success != c.try_pop( v2) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for_closed() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    c.close();
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
    BOOST_CHECK( boost::fibers::channel_op_status::closed == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
}

void test_pop_wait_for_success() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_for( v2, std::chrono::seconds( 1) ) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_for_timeout() {
    boost::fibers::bounded_channel< int > c( 10);
    int v = 0;
    boost::fibers::fiber f( boost::fibers::launch::post, [&c,&v](){
        BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.pop_wait_for( v, std::chrono::seconds( 1) ) );
    });
    f.join();
}

void test_pop_wait_until() {
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
            std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_until_closed() {
    boost::fibers::bounded_channel< int > c( 10);
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
    boost::fibers::bounded_channel< int > c( 10);
    int v1 = 2, v2 = 0;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&v2](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.pop_wait_until( v2,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    });
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,v1](){
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( v1) );
    });
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( v1, v2);
}

void test_pop_wait_until_timeout() {
    boost::fibers::bounded_channel< int > c( 10);
    int v = 0;
    boost::fibers::fiber f( boost::fibers::launch::post, [&c,&v](){
        BOOST_CHECK( boost::fibers::channel_op_status::timeout == c.pop_wait_until( v,
                    std::chrono::system_clock::now() + std::chrono::seconds( 1) ) );
    });
    f.join();
}

void test_wm_1() {
    boost::fibers::bounded_channel< int > c( 3);
    std::vector< boost::fibers::fiber::id > ids;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&ids](){
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
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,&ids](){
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
    BOOST_CHECK_EQUAL( id1, ids[0]); // f1 pushes 1
    BOOST_CHECK_EQUAL( id1, ids[1]); // f1 pushes 2
    BOOST_CHECK_EQUAL( id1, ids[2]); // f1 pushes 3
    BOOST_CHECK_EQUAL( id1, ids[3]); // f1 blocks in push( 4) (channel is full)
    BOOST_CHECK_EQUAL( id2, ids[4]); // f2 resumes and pops 1, f1 gets ready to push 4, f2 yields
    BOOST_CHECK_EQUAL( id1, ids[5]); // f1 resumes and pushes 4, blocks in push( 5) (channel full)
    BOOST_CHECK_EQUAL( id2, ids[6]); // f2 resumes and pops 2
    BOOST_CHECK_EQUAL( id2, ids[7]); // f2 pops 3
    BOOST_CHECK_EQUAL( id2, ids[8]); // f2 pops 4
    BOOST_CHECK_EQUAL( id2, ids[9]); // f2 blocks in pop() (channel is empty)
    BOOST_CHECK_EQUAL( id1, ids[10]); // f1 resumes and pushes 4, completes
    BOOST_CHECK_EQUAL( id2, ids[11]); // f2 resumes and pops 5, completes
}

void test_wm_2() {
    boost::fibers::bounded_channel< int > c( 3);
    std::vector< boost::fibers::fiber::id > ids;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&ids](){
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
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,&ids](){
        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 1, c.value_pop() );

        // let other fiber run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 2, c.value_pop() );

        // let other fiber run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 3, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 4, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 5, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
    });
    boost::fibers::fiber::id id1 = f1.get_id();
    boost::fibers::fiber::id id2 = f2.get_id();
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( 12u, ids.size() );
    BOOST_CHECK_EQUAL( id1, ids[0]); // f1 pushes 1
    BOOST_CHECK_EQUAL( id1, ids[1]); // f1 pushes 2
    BOOST_CHECK_EQUAL( id1, ids[2]); // f1 pushes 3
    BOOST_CHECK_EQUAL( id1, ids[3]); // f1 blocks in push( 4) (channel is full)
    BOOST_CHECK_EQUAL( id2, ids[4]); // f2 resumes and pops 1, f1 gets ready to push 4, f2 yields
    BOOST_CHECK_EQUAL( id1, ids[5]); // f1 resumes and pushes 4, blocks in push( 5) (channel full)
    BOOST_CHECK_EQUAL( id2, ids[6]); // f2 resumes and pops 2, f1 gets ready tp push 5, f2 yields
    BOOST_CHECK_EQUAL( id1, ids[7]); // f1 resumes and pushes 5, completes
    BOOST_CHECK_EQUAL( id2, ids[8]); // f2 resumes and pops 3
    BOOST_CHECK_EQUAL( id2, ids[9]); // f2 pops 4
    BOOST_CHECK_EQUAL( id2, ids[10]); // f2 pops 5
    BOOST_CHECK_EQUAL( id2, ids[11]); // f2 completes
}

void test_wm_3() {
    boost::fibers::bounded_channel< int > c( 3, 1);
    BOOST_CHECK_EQUAL( c.upper_bound(), 3u );
    BOOST_CHECK_EQUAL( c.lower_bound(), 1u );
    std::vector< boost::fibers::fiber::id > ids;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&ids](){
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
        BOOST_CHECK( boost::fibers::channel_op_status::success == c.push( 5) );

        ids.push_back( boost::this_fiber::get_id() );
    });
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,&ids](){
        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 1, c.value_pop() );

        // let other fiber run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 2, c.value_pop() );

        // let other fiber run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 3, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 4, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 5, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
    });
    boost::fibers::fiber::id id1 = f1.get_id();
    boost::fibers::fiber::id id2 = f2.get_id();
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( 12u, ids.size() );
    BOOST_CHECK_EQUAL( id1, ids[0]); // f1 pushes 1
    BOOST_CHECK_EQUAL( id1, ids[1]); // f1 pushes 2
    BOOST_CHECK_EQUAL( id1, ids[2]); // f1 pushes 3
    BOOST_CHECK_EQUAL( id1, ids[3]); // f1 blocks in push( 4) (channel is full)
    BOOST_CHECK_EQUAL( id2, ids[4]); // f2 resumes and pops 1, f1 gets NOT ready to push 4, f2 yields
    BOOST_CHECK_EQUAL( id2, ids[5]); // f2 pops 2, f1 gets ready to push 4 (lwm == size == 1), f2 yields
    BOOST_CHECK_EQUAL( id1, ids[6]); // f1 resumes and pushes 4 + 5
    BOOST_CHECK_EQUAL( id1, ids[7]); // f1 completes
    BOOST_CHECK_EQUAL( id2, ids[8]); // f2 resumes and pops 3
    BOOST_CHECK_EQUAL( id2, ids[9]); // f2 pops 4
    BOOST_CHECK_EQUAL( id2, ids[10]); // f2 pops 5
    BOOST_CHECK_EQUAL( id2, ids[11]); // f2 completes
}

void test_wm_4() {
    boost::fibers::bounded_channel< int > c( 3, 1);
    std::vector< boost::fibers::fiber::id > ids;
    boost::fibers::fiber f1( boost::fibers::launch::post, [&c,&ids](){
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
    });
    boost::fibers::fiber f2( boost::fibers::launch::post, [&c,&ids](){
        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 1, c.value_pop() );

        // let potential other fibers run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 2, c.value_pop() );

        // let potential other fibers run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        BOOST_CHECK_EQUAL( 3, c.value_pop() );

        // let potential other fibers run
        boost::this_fiber::yield();

        ids.push_back( boost::this_fiber::get_id() );
        // would block because channel is empty
        BOOST_CHECK_EQUAL( 4, c.value_pop() );

        ids.push_back( boost::this_fiber::get_id() );
    });
    boost::fibers::fiber::id id1 = f1.get_id();
    boost::fibers::fiber::id id2 = f2.get_id();
    f1.join();
    f2.join();
    BOOST_CHECK_EQUAL( 10u, ids.size() );
    BOOST_CHECK_EQUAL( id1, ids[0]); // f1 pushes 1
    BOOST_CHECK_EQUAL( id1, ids[1]); // f1 pushes 2
    BOOST_CHECK_EQUAL( id1, ids[2]); // f1 pushes 3
    BOOST_CHECK_EQUAL( id1, ids[3]); // f1 blocks in push( 4) ( channel full)
    BOOST_CHECK_EQUAL( id2, ids[4]); // f2 resumes and pops 1, f1 gets NOT ready to push 4, f2 yields
    BOOST_CHECK_EQUAL( id2, ids[5]); // f2 pops 2, f1 gets ready to push 4 (lwm == size == 1), f2 yields
    BOOST_CHECK_EQUAL( id1, ids[6]); // f1 resumes and pushes 4, completes
    BOOST_CHECK_EQUAL( id2, ids[7]); // f2 resumes and pops 3
    BOOST_CHECK_EQUAL( id2, ids[8]); // f2 pops 4
    BOOST_CHECK_EQUAL( id2, ids[9]); // f2 completes
}

void test_moveable() {
    boost::fibers::bounded_channel< moveable > c( 10);
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
        BOOST_TEST_SUITE("Boost.Fiber: bounded_channel test suite");

     test->add( BOOST_TEST_CASE( & test_zero_wm_1) );
     test->add( BOOST_TEST_CASE( & test_zero_wm_2) );
     test->add( BOOST_TEST_CASE( & test_hwm_less_lwm) );
     test->add( BOOST_TEST_CASE( & test_hwm_equal_lwm) );
     test->add( BOOST_TEST_CASE( & test_push) );
     test->add( BOOST_TEST_CASE( & test_push_closed) );
     test->add( BOOST_TEST_CASE( & test_try_push) );
     test->add( BOOST_TEST_CASE( & test_try_push_closed) );
     test->add( BOOST_TEST_CASE( & test_try_push_full) );
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
     test->add( BOOST_TEST_CASE( & test_wm_1) );
     test->add( BOOST_TEST_CASE( & test_wm_2) );
     test->add( BOOST_TEST_CASE( & test_wm_3) );
     test->add( BOOST_TEST_CASE( & test_wm_4) );
     test->add( BOOST_TEST_CASE( & test_moveable) );

    return test;
}
