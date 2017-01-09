
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// This test is based on the tests of Boost.Thread

#include <chrono>
#include <mutex>
#include <sstream>
#include <string>

#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/fiber/all.hpp>

int value1 = 0;
std::string value2 = "";

struct X {
    int value;

    void foo( int i) {
        value = i;
    }
};

class copyable {
public:
    bool    state;
    int     value;

    copyable() :
        state( false),
        value( -1) {
    }

    copyable( int v) :
        state( true),
        value( v) {
    }

    void operator()() {
        value1 = value;
    }
};

class moveable {
public:
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
        value = other.value;
        other.state = false;
        other.value = -1;
        return * this;
    }

    moveable( moveable const& other) = delete;
    moveable & operator=( moveable const& other) = delete;

    void operator()() {
        value1 = value;
    }
};

class detachable {
private:
    int   alive_count_;

public:
    static int    alive_count;
    static bool   was_running;

    detachable() :
        alive_count_( 1) {
        ++alive_count;
    }

    detachable( detachable const& g) :
        alive_count_( g.alive_count_) {
        ++alive_count;
    }

    ~detachable() {
        alive_count_ = 0;
        --alive_count;
    }

    void operator()() {
        BOOST_CHECK_EQUAL(1, alive_count_);
        was_running = true;
    }
};

int detachable::alive_count = 0;
bool detachable::was_running = false;

void fn1() {
    value1 = 1;
}

void fn2( int i, std::string const& s) {
    value1 = i;
    value2 = s;
}

void fn3( int & i) {
    i = 1;
    boost::this_fiber::yield();
    i = 1;
    boost::this_fiber::yield();
    i = 2;
    boost::this_fiber::yield();
    i = 3;
    boost::this_fiber::yield();
    i = 5;
    boost::this_fiber::yield();
    i = 8;
}

void fn4() {
    boost::this_fiber::yield();
}

void fn5() {
    boost::fibers::fiber f( boost::fibers::launch::post, fn4);
    BOOST_CHECK( f.joinable() );
    f.join();
    BOOST_CHECK( ! f.joinable() );
}

void test_scheduler_dtor() {
    boost::fibers::context * ctx(
        boost::fibers::context::active() );
    (void)ctx;
}

void test_join_fn() {
    {
        value1 = 0;
        boost::fibers::fiber f( boost::fibers::launch::post, fn1);
        f.join();
        BOOST_CHECK_EQUAL( value1, 1);
    }
    {
        value1 = 0;
        value2 = "";
        boost::fibers::fiber f( boost::fibers::launch::post, fn2, 3, "abc");
        f.join();
        BOOST_CHECK_EQUAL( value1, 3);
        BOOST_CHECK_EQUAL( value2, "abc");
    }
}

void test_join_memfn() {
    X x = {0};
    BOOST_CHECK_EQUAL( x.value, 0);
    boost::fibers::fiber( boost::fibers::launch::post, & X::foo, std::ref( x), 3).join();
    BOOST_CHECK_EQUAL( x.value, 3);
}

void test_join_copyable() {
    value1 = 0;
    copyable cp( 3);
    BOOST_CHECK( cp.state);
    BOOST_CHECK_EQUAL( value1, 0);
    boost::fibers::fiber f( boost::fibers::launch::post, cp);
    f.join();
    BOOST_CHECK( cp.state);
    BOOST_CHECK_EQUAL( value1, 3);
}

void test_join_moveable() {
    value1 = 0;
    moveable mv( 7);
    BOOST_CHECK( mv.state);
    BOOST_CHECK_EQUAL( value1, 0);
    boost::fibers::fiber f( boost::fibers::launch::post, std::move( mv) );
    f.join();
    BOOST_CHECK( ! mv.state);
    BOOST_CHECK_EQUAL( value1, 7);
}

void test_join_lambda() {
    {
        value1 = 0;
        value2 = "";
        int i = 3;
        std::string abc("abc");
        boost::fibers::fiber f(
                boost::fibers::launch::post, [i,abc]() {
                    value1 = i;
                    value2 = abc;
                });
        f.join();
        BOOST_CHECK_EQUAL( value1, 3);
        BOOST_CHECK_EQUAL( value2, "abc");
    }
    {
        value1 = 0;
        value2 = "";
        int i = 3;
        std::string abc("abc");
        boost::fibers::fiber f(
                boost::fibers::launch::post, [](int i, std::string const& abc) {
                    value1 = i;
                    value2 = abc;
                },
                i, abc);
        f.join();
        BOOST_CHECK_EQUAL( value1, 3);
        BOOST_CHECK_EQUAL( value2, "abc");
    }
}

void test_join_bind() {
    {
        value1 = 0;
        value2 = "";
        int i = 3;
        std::string abc("abc");
        boost::fibers::fiber f(
            boost::fibers::launch::post, std::bind(
                [i,abc]() {
                    value1 = i;
                    value2 = abc;
                }
            ));
        f.join();
        BOOST_CHECK_EQUAL( value1, 3);
        BOOST_CHECK_EQUAL( value2, "abc");
    }
    {
        value1 = 0;
        value2 = "";
        int i = 3;
        std::string abc("abc");
        boost::fibers::fiber f(
            boost::fibers::launch::post, std::bind(
                []( int i, std::string & str) {
                    value1 = 3;
                    value2 = str;
                },
                i, abc
            ));
        f.join();
        BOOST_CHECK_EQUAL( value1, 3);
        BOOST_CHECK_EQUAL( value2, "abc");
    }
#if 0
    {
        value1 = 0;
        value2 = "";
        int i = 3;
        std::string abc("abc");
        boost::fibers::fiber f(
            boost::fibers::launch::post, std::bind(
                []( int i, std::string & str) {
                    value1 = 3;
                    value2 = str;
                },
                std::placeholders::_1,
                std::placeholders::_2
            ),
            i, abc);
        f.join();
        BOOST_CHECK_EQUAL( value1, 3);
        BOOST_CHECK_EQUAL( value2, "abc");
    }
#endif
}

void test_join_in_fiber() {
    // spawn fiber f
    // f spawns an new fiber f' in its fiber-fn
    // f' yields in its fiber-fn
    // f joins s' and gets suspended (waiting on s')
    boost::fibers::fiber f( boost::fibers::launch::post, fn5);
    BOOST_CHECK( f.joinable() );
    // join() resumes f + f' which completes
    f.join();
    BOOST_CHECK( ! f.joinable() );
}

void test_move_fiber() {
    boost::fibers::fiber f1;
    BOOST_CHECK( ! f1.joinable() );
    boost::fibers::fiber f2( boost::fibers::launch::post, fn1);
    BOOST_CHECK( f2.joinable() );
    f1 = std::move( f2);
    BOOST_CHECK( f1.joinable() );
    BOOST_CHECK( ! f2.joinable() );
    f1.join();
    BOOST_CHECK( ! f1.joinable() );
    BOOST_CHECK( ! f2.joinable() );
}

void test_id() {
    boost::fibers::fiber f1;
    boost::fibers::fiber f2( boost::fibers::launch::post, fn1);
    BOOST_CHECK( ! f1.joinable() );
    BOOST_CHECK( f2.joinable() );

    BOOST_CHECK_EQUAL( boost::fibers::fiber::id(), f1.get_id() );
    BOOST_CHECK( boost::fibers::fiber::id() != f2.get_id() );

    boost::fibers::fiber f3( boost::fibers::launch::post, fn1);
    BOOST_CHECK( f2.get_id() != f3.get_id() );

    f1 = std::move( f2);
    BOOST_CHECK( f1.joinable() );
    BOOST_CHECK( ! f2.joinable() );

    BOOST_CHECK( boost::fibers::fiber::id() != f1.get_id() );
    BOOST_CHECK_EQUAL( boost::fibers::fiber::id(), f2.get_id() );

    BOOST_CHECK( ! f2.joinable() );

    f1.join();
    f3.join();
}

void test_yield() {
    int v1 = 0, v2 = 0;
    BOOST_CHECK_EQUAL( 0, v1);
    BOOST_CHECK_EQUAL( 0, v2);
    boost::fibers::fiber f1( boost::fibers::launch::post, fn3, std::ref( v1) );
    boost::fibers::fiber f2( boost::fibers::launch::post, fn3, std::ref( v2) );
    f1.join();
    f2.join();
    BOOST_CHECK( ! f1.joinable() );
    BOOST_CHECK( ! f2.joinable() );
    BOOST_CHECK_EQUAL( 8, v1);
    BOOST_CHECK_EQUAL( 8, v2);
}

void test_sleep_for() {
    typedef std::chrono::system_clock Clock;
    typedef Clock::time_point time_point;
    std::chrono::milliseconds ms(500);
    time_point t0 = Clock::now();
    boost::this_fiber::sleep_for(ms);
    time_point t1 = Clock::now();
    std::chrono::nanoseconds ns = (t1 - t0) - ms;
    std::chrono::nanoseconds err = ms / 100;
    // The time slept is within 1% of 500ms
    // This test is spurious as it depends on the time the fiber system switches the fiber
    BOOST_CHECK((std::max)(ns.count(), -ns.count()) < (err+std::chrono::milliseconds(1000)).count());
    //BOOST_TEST(std::abs(static_cast<long>(ns.count())) < (err+std::chrono::milliseconds(1000)).count());
}

void test_sleep_until() {
    {
        typedef std::chrono::steady_clock Clock;
        typedef Clock::time_point time_point;
        std::chrono::milliseconds ms(500);
        time_point t0 = Clock::now();
        boost::this_fiber::sleep_until(t0 + ms);
        time_point t1 = Clock::now();
        std::chrono::nanoseconds ns = (t1 - t0) - ms;
        std::chrono::nanoseconds err = ms / 100;
        // The time slept is within 1% of 500ms
        // This test is spurious as it depends on the time the thread system switches the threads
        BOOST_CHECK((std::max)(ns.count(), -ns.count()) < (err+std::chrono::milliseconds(1000)).count());
        //BOOST_TEST(std::abs(static_cast<long>(ns.count())) < (err+std::chrono::milliseconds(1000)).count());
    }
    {
        typedef std::chrono::system_clock Clock;
        typedef Clock::time_point time_point;
        std::chrono::milliseconds ms(500);
        time_point t0 = Clock::now();
        boost::this_fiber::sleep_until(t0 + ms);
        time_point t1 = Clock::now();
        std::chrono::nanoseconds ns = (t1 - t0) - ms;
        std::chrono::nanoseconds err = ms / 100;
        // The time slept is within 1% of 500ms
        // This test is spurious as it depends on the time the thread system switches the threads
        BOOST_CHECK((std::max)(ns.count(), -ns.count()) < (err+std::chrono::milliseconds(1000)).count());
        //BOOST_TEST(std::abs(static_cast<long>(ns.count())) < (err+std::chrono::milliseconds(1000)).count());
    }
}

void do_wait( boost::fibers::barrier* b) {
    b->wait();
}

void test_detach() {
    {
        boost::fibers::fiber f( boost::fibers::launch::post, (detachable()) );
        BOOST_CHECK( f.joinable() );
        f.detach();
        BOOST_CHECK( ! f.joinable() );
        boost::this_fiber::sleep_for( std::chrono::milliseconds(250) );
        BOOST_CHECK( detachable::was_running);
        BOOST_CHECK_EQUAL( 0, detachable::alive_count);
    }
    {
        boost::fibers::fiber f( boost::fibers::launch::post, (detachable()) );
        BOOST_CHECK( f.joinable() );
        boost::this_fiber::yield();
        f.detach();
        BOOST_CHECK( ! f.joinable() );
        boost::this_fiber::sleep_for( std::chrono::milliseconds(250) );
        BOOST_CHECK( detachable::was_running);
        BOOST_CHECK_EQUAL( 0, detachable::alive_count);
    }
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* []) {
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Fiber: fiber test suite");

    test->add( BOOST_TEST_CASE( & test_scheduler_dtor) );
    test->add( BOOST_TEST_CASE( & test_join_fn) );
    test->add( BOOST_TEST_CASE( & test_join_memfn) );
    test->add( BOOST_TEST_CASE( & test_join_copyable) );
    test->add( BOOST_TEST_CASE( & test_join_moveable) );
    test->add( BOOST_TEST_CASE( & test_join_lambda) );
    test->add( BOOST_TEST_CASE( & test_join_bind) );
    test->add( BOOST_TEST_CASE( & test_join_in_fiber) );
    test->add( BOOST_TEST_CASE( & test_move_fiber) );
    test->add( BOOST_TEST_CASE( & test_move_fiber) );
    test->add( BOOST_TEST_CASE( & test_yield) );
    test->add( BOOST_TEST_CASE( & test_sleep_for) );
    test->add( BOOST_TEST_CASE( & test_sleep_until) );
    test->add( BOOST_TEST_CASE( & test_detach) );

    return test;
}
