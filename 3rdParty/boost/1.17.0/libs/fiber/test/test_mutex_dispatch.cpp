
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// This test is based on the tests of Boost.Thread 

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <boost/fiber/all.hpp>

typedef std::chrono::nanoseconds  ns;
typedef std::chrono::milliseconds ms;

int value1 = 0;
int value2 = 0;

template< typename M >
void fn1( M & mtx) {
    typedef M mutex_type;
	typename std::unique_lock< mutex_type > lk( mtx);
	++value1;
	for ( int i = 0; i < 3; ++i)
		boost::this_fiber::yield();
}

template< typename M >
void fn2( M & mtx) {
    typedef M mutex_type;
	++value2;
	typename std::unique_lock< mutex_type > lk( mtx);
	++value2;
}

void fn3( boost::fibers::timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    m.lock();
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(2500000)+ms(2000)); // within 2.5 ms
}

void fn4( boost::fibers::timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    while ( ! m.try_lock() );
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(50000000)+ms(2000)); // within 50 ms
}

void fn5( boost::fibers::timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK( m.try_lock_for(ms(300)+ms(2000)) == true);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(5000000)+ms(2000)); // within 5 ms
}

void fn6( boost::fibers::timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock_for(ms(250)) == false);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(5000000)+ms(2000)); // within 5 ms
}

void fn7( boost::fibers::timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock_until(std::chrono::steady_clock::now() + ms(300) + ms(1000)) == true);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(5000000)+ms(2000)); // within 5ms
}

void fn8( boost::fibers::timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock_until(std::chrono::steady_clock::now() + ms(250)) == false);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    ns d = t1 - t0 - ms(250);
    ns r = ns(5000000)+ms(2000); // within 6ms
    BOOST_CHECK(d < r); // within 6ms
}

void fn9( boost::fibers::recursive_timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    m.lock();
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.lock();
    m.unlock();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ms(2500)+ms(2000)); // within 2.5 ms
}

void fn10( boost::fibers::recursive_timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    while (!m.try_lock()) ;
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock());
    m.unlock();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ms(50000)+ms(2000)); // within 50 ms
}

void fn11( boost::fibers::recursive_timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock_for(ms(300)+ms(1000)) == true);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock());
    m.unlock();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(5000000)+ms(2000)); // within 5 ms
}

void fn12( boost::fibers::recursive_timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock_for(ms(250)) == false);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ms(5000)+ms(2000)); // within 5 ms
}

void fn13( boost::fibers::recursive_timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock_until(std::chrono::steady_clock::now() + ms(300) + ms(1000)) == true);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(5000000)+ms(2000)); // within 5 ms
}

void fn14( boost::fibers::recursive_timed_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock_until(std::chrono::steady_clock::now() + ms(250)) == false);
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(5000000)+ms(2000)); // within 5 ms
}

void fn15( boost::fibers::recursive_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    m.lock();
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.lock();
    m.unlock();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(2500000)+ms(2000)); // within 2.5 ms
}

void fn16( boost::fibers::recursive_mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    while (!m.try_lock());
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    BOOST_CHECK(m.try_lock());
    m.unlock();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(50000000)+ms(2000)); // within 50 ms
}

void fn17( boost::fibers::mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    m.lock();
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ms(2500)+ms(2000)); // within 2.5 ms
}

void fn18( boost::fibers::mutex & m) {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    while (!m.try_lock()) ;
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    m.unlock();
    ns d = t1 - t0 - ms(250);
    BOOST_CHECK(d < ns(50000000)+ms(2000)); // within 50 ms
}

template< typename M >
struct test_lock {
    typedef M mutex_type;
    typedef typename std::unique_lock< M > lock_type;

    void operator()() {
        mutex_type mtx;

        // Test the lock's constructors.
        {
            lock_type lk(mtx, std::defer_lock);
            BOOST_CHECK(!lk);
        }
        lock_type lk(mtx);
        BOOST_CHECK(lk ? true : false);

        // Test the lock and unlock methods.
        lk.unlock();
        BOOST_CHECK(!lk);
        lk.lock();
        BOOST_CHECK(lk ? true : false);
    }
};

template< typename M >
struct test_exclusive {
    typedef M mutex_type;
    typedef typename std::unique_lock< M > lock_type;

    void operator()() {
        value1 = 0;
        value2 = 0;
        BOOST_CHECK_EQUAL( 0, value1);
        BOOST_CHECK_EQUAL( 0, value2);

        mutex_type mtx;
        boost::fibers::fiber f1( boost::fibers::launch::dispatch, & fn1< mutex_type >, std::ref( mtx) );
        boost::fibers::fiber f2( boost::fibers::launch::dispatch, & fn2< mutex_type >, std::ref( mtx) );
        BOOST_ASSERT( f1.joinable() );
        BOOST_ASSERT( f2.joinable() );

        f1.join();
        f2.join();
        BOOST_CHECK_EQUAL( 1, value1);
        BOOST_CHECK_EQUAL( 2, value2);
    }
};

template< typename M >
struct test_recursive_lock {
    typedef M mutex_type;
    typedef typename std::unique_lock< M > lock_type;

    void operator()() {
        mutex_type mx;
        lock_type lock1(mx);
        lock_type lock2(mx);
    }
};

void do_test_mutex() {
    test_lock< boost::fibers::mutex >()();
    test_exclusive< boost::fibers::mutex >()();

    {
        boost::fibers::mutex mtx;
        mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn17, std::ref( mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        mtx.unlock();
        f.join();
    }

    {
        boost::fibers::mutex mtx;
        mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn18, std::ref( mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        mtx.unlock();
        f.join();
    }
}

void test_mutex() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_mutex).join();
}

void do_test_recursive_mutex() {
    test_lock< boost::fibers::recursive_mutex >()();
    test_exclusive< boost::fibers::recursive_mutex >()();
    test_recursive_lock< boost::fibers::recursive_mutex >()();

    {
        boost::fibers::recursive_mutex mtx;
        mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn15, std::ref( mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        mtx.unlock();
        f.join();
    }

    {
        boost::fibers::recursive_mutex mtx;
        mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn16, std::ref( mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        mtx.unlock();
        f.join();
    }
}

void test_recursive_mutex() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, do_test_recursive_mutex).join();
}

void do_test_timed_mutex() {
    test_lock< boost::fibers::timed_mutex >()();
    test_exclusive< boost::fibers::timed_mutex >()();

    {
        boost::fibers::timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn3, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn4, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn5, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn6, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(300) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn7, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn8, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(300) + ms(1000) );
        timed_mtx.unlock();
        f.join();
    }
}

void test_timed_mutex() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_timed_mutex).join();
}

void do_test_recursive_timed_mutex() {
    test_lock< boost::fibers::recursive_timed_mutex >()();
    test_exclusive< boost::fibers::recursive_timed_mutex >()();
    test_recursive_lock< boost::fibers::recursive_timed_mutex >()();

    {
        boost::fibers::recursive_timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn9, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::recursive_timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn10, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::recursive_timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn11, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::recursive_timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn12, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(400) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::recursive_timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn13, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(250) );
        timed_mtx.unlock();
        f.join();
    }

    {
        boost::fibers::recursive_timed_mutex timed_mtx;
        timed_mtx.lock();
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn14, std::ref( timed_mtx) );
        boost::this_fiber::sleep_for( ms(300) );
        timed_mtx.unlock();
        f.join();
    }
}

void test_recursive_timed_mutex() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_recursive_timed_mutex).join();
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* []) {
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Fiber: mutex test suite");

    test->add( BOOST_TEST_CASE( & test_mutex) );
    test->add( BOOST_TEST_CASE( & test_recursive_mutex) );
    test->add( BOOST_TEST_CASE( & test_timed_mutex) );
    test->add( BOOST_TEST_CASE( & test_recursive_timed_mutex) );

	return test;
}
