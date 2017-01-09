
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// This test is based on the tests of Boost.Thread 

#include <chrono>
#include <cstdio>
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

int value = 0;

inline
std::chrono::system_clock::time_point delay(int secs, int msecs=0, int nsecs=0) {
    std::chrono::system_clock::time_point t = std::chrono::system_clock::now();
    t += std::chrono::seconds( secs);
    t += std::chrono::milliseconds( msecs);
    //t += std::chrono::nanoseconds( nsecs);

    return t;
}

struct condition_test_data {
    condition_test_data() : notified(0), awoken(0) { }

    boost::fibers::mutex mutex;
    boost::fibers::condition_variable cond;
    int notified;
    int awoken;
};

void condition_test_fiber(condition_test_data* data) {
    std::unique_lock<boost::fibers::mutex> lock(data->mutex);
    BOOST_CHECK(lock ? true : false);
    while (!(data->notified > 0))
        data->cond.wait(lock);
    BOOST_CHECK(lock ? true : false);
    data->awoken++;
}

struct cond_predicate {
    cond_predicate(int& var, int val) : _var(var), _val(val) { }

    bool operator()() { return _var == _val; }

    int& _var;
    int _val;
private:
    void operator=(cond_predicate&);
    
};

void notify_one_fn( boost::fibers::condition_variable & cond) {
	cond.notify_one();
}

void notify_all_fn( boost::fibers::condition_variable & cond) {
	cond.notify_all();
}

void wait_fn(
	boost::fibers::mutex & mtx,
	boost::fibers::condition_variable & cond) {
	std::unique_lock< boost::fibers::mutex > lk( mtx);
	cond.wait( lk);
	++value;
}

void test_one_waiter_notify_one() {
	value = 0;
	boost::fibers::mutex mtx;
	boost::fibers::condition_variable cond;

    boost::fibers::fiber f1(
                boost::fibers::launch::dispatch,
                wait_fn,
                std::ref( mtx),
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 0, value);

	boost::fibers::fiber f2(
                boost::fibers::launch::dispatch,
                notify_one_fn,
                std::ref( cond) );

	BOOST_CHECK_EQUAL( 0, value);

    f1.join();
    f2.join();

	BOOST_CHECK_EQUAL( 1, value);
}

void test_two_waiter_notify_one() {
	value = 0;
	boost::fibers::mutex mtx;
	boost::fibers::condition_variable cond;

    boost::fibers::fiber f1(
                boost::fibers::launch::dispatch,
                wait_fn,
                std::ref( mtx),
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 0, value);

    boost::fibers::fiber f2(
                boost::fibers::launch::dispatch,
                wait_fn,
                std::ref( mtx),
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 0, value);

    boost::fibers::fiber f3(
                boost::fibers::launch::dispatch,
                notify_one_fn,
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 0, value);

    boost::fibers::fiber f4(
                boost::fibers::launch::dispatch,
                notify_one_fn,
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 1, value);

    f1.join();
    f2.join();
    f3.join();
    f4.join();

	BOOST_CHECK_EQUAL( 2, value);
}

void test_two_waiter_notify_all() {
	value = 0;
	boost::fibers::mutex mtx;
	boost::fibers::condition_variable cond;

    boost::fibers::fiber f1(
                boost::fibers::launch::dispatch,
                wait_fn,
                std::ref( mtx),
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 0, value);

    boost::fibers::fiber f2(
                boost::fibers::launch::dispatch,
                wait_fn,
                std::ref( mtx),
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 0, value);

    boost::fibers::fiber f3(
                boost::fibers::launch::dispatch,
                notify_all_fn,
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 0, value);

    boost::fibers::fiber f4(
                boost::fibers::launch::dispatch,
                wait_fn,
                std::ref( mtx),
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 2, value);

    boost::fibers::fiber f5(
                boost::fibers::launch::dispatch,
                notify_all_fn,
                std::ref( cond) );
	BOOST_CHECK_EQUAL( 2, value);

    f1.join();
    f2.join();
    f3.join();
    f4.join();
    f5.join();

	BOOST_CHECK_EQUAL( 3, value);
}

int test1 = 0;
int test2 = 0;

int runs = 0;

void fn1( boost::fibers::mutex & m, boost::fibers::condition_variable & cv) {
    std::unique_lock< boost::fibers::mutex > lk( m);
    BOOST_CHECK(test2 == 0);
    test1 = 1;
    cv.notify_one();
    while (test2 == 0) {
        cv.wait(lk);
    }
    BOOST_CHECK(test2 != 0);
}

void fn2( boost::fibers::mutex & m, boost::fibers::condition_variable & cv) {
    std::unique_lock< boost::fibers::mutex > lk( m);
    BOOST_CHECK(test2 == 0);
    test1 = 1;
    cv.notify_one();
    std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point t = t0 + ms(250);
    int count=0;
    while (test2 == 0 && cv.wait_until(lk, t) == boost::fibers::cv_status::no_timeout)
        count++;
    std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();
    if (runs == 0) {
        BOOST_CHECK(t1 - t0 < ms(250));
        BOOST_CHECK(test2 != 0);
    } else {
        BOOST_CHECK(t1 - t0 - ms(250) < ms(count*250+5+1000));
        BOOST_CHECK(test2 == 0);
    }
    ++runs;
}

class Pred {
     int    &   i_;

public:
    explicit Pred(int& i) :
        i_(i)
    {}

    bool operator()()
    { return i_ != 0; }
};

void fn3( boost::fibers::mutex & m, boost::fibers::condition_variable & cv) {
    std::unique_lock< boost::fibers::mutex > lk( m);
    BOOST_CHECK(test2 == 0);
    test1 = 1;
    cv.notify_one();
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point t = t0 + ms(250);
    bool r = cv.wait_until(lk, t, Pred(test2));
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    if (runs == 0) {
        BOOST_CHECK(t1 - t0 < ms(250));
        BOOST_CHECK(test2 != 0);
        BOOST_CHECK(r);
    } else {
        BOOST_CHECK(t1 - t0 - ms(250) < ms(250+2));
        BOOST_CHECK(test2 == 0);
        BOOST_CHECK(!r);
    }
    ++runs;
}

void fn4( boost::fibers::mutex & m, boost::fibers::condition_variable & cv) {
    std::unique_lock< boost::fibers::mutex > lk( m);
    BOOST_CHECK(test2 == 0);
    test1 = 1;
    cv.notify_one();
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    int count=0;
    while (test2 == 0 && cv.wait_for(lk, ms(250)) == boost::fibers::cv_status::no_timeout)
        count++;
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    if (runs == 0) {
        BOOST_CHECK(t1 - t0 < ms(250));
        BOOST_CHECK(test2 != 0);
    } else {
        BOOST_CHECK(t1 - t0 - ms(250) < ms(count*250+5+1000));
        BOOST_CHECK(test2 == 0);
    }
    ++runs;
}

void fn5( boost::fibers::mutex & m, boost::fibers::condition_variable & cv) {
    std::unique_lock< boost::fibers::mutex > lk( m);
    BOOST_CHECK(test2 == 0);
    test1 = 1;
    cv.notify_one();
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    int count=0;
    cv.wait_for(lk, ms(250), Pred(test2));
    count++;
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    if (runs == 0) {
        BOOST_CHECK(t1 - t0 < ms(250+1000));
        BOOST_CHECK(test2 != 0);
    } else {
        BOOST_CHECK(t1 - t0 - ms(250) < ms(count*250+2));
        BOOST_CHECK(test2 == 0);
    }
    ++runs;
}

void do_test_condition_wait() {
    test1 = 0;
    test2 = 0;
    runs = 0;

    boost::fibers::mutex m;
    boost::fibers::condition_variable cv;
    std::unique_lock< boost::fibers::mutex > lk( m);
    boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn1, std::ref( m), std::ref( cv) );
    BOOST_CHECK(test1 == 0);
    while (test1 == 0)
        cv.wait(lk);
    BOOST_CHECK(test1 != 0);
    test2 = 1;
    lk.unlock();
    cv.notify_one();
    f.join();
}

void test_condition_wait() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_condition_wait).join();
    do_test_condition_wait();
}

void do_test_condition_wait_until() {
    test1 = 0;
    test2 = 0;
    runs = 0;

    boost::fibers::mutex m;
    boost::fibers::condition_variable cv;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn2, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        test2 = 1;
        lk.unlock();
        cv.notify_one();
        f.join();
    }
    test1 = 0;
    test2 = 0;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn2, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        lk.unlock();
        f.join();
    }
}

void test_condition_wait_until() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_condition_wait_until).join();
    do_test_condition_wait_until();
}

void do_test_condition_wait_until_pred() {
    test1 = 0;
    test2 = 0;
    runs = 0;

    boost::fibers::mutex m;
    boost::fibers::condition_variable cv;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn3, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        test2 = 1;
        lk.unlock();
        cv.notify_one();
        f.join();
    }
    test1 = 0;
    test2 = 0;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn3, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        lk.unlock();
        f.join();
    }
}

void test_condition_wait_until_pred() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_condition_wait_until_pred).join();
    do_test_condition_wait_until_pred();
}

void do_test_condition_wait_for() {
    test1 = 0;
    test2 = 0;
    runs = 0;

    boost::fibers::mutex m;
    boost::fibers::condition_variable cv;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn4, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        test2 = 1;
        lk.unlock();
        cv.notify_one();
        f.join();
    }
    test1 = 0;
    test2 = 0;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn4, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        lk.unlock();
        f.join();
    }
}

void test_condition_wait_for() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_condition_wait_for).join();
    do_test_condition_wait_for();
}

void do_test_condition_wait_for_pred() {
    test1 = 0;
    test2 = 0;
    runs = 0;

    boost::fibers::mutex m;
    boost::fibers::condition_variable cv;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn5, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        test2 = 1;
        lk.unlock();
        cv.notify_one();
        f.join();
    }
    test1 = 0;
    test2 = 0;
    {
        std::unique_lock< boost::fibers::mutex > lk( m);
        boost::fibers::fiber f( boost::fibers::launch::dispatch, & fn5, std::ref( m), std::ref( cv) );
        BOOST_CHECK(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_CHECK(test1 != 0);
        lk.unlock();
        f.join();
    }
}

void test_condition_wait_for_pred() {
    boost::fibers::fiber( boost::fibers::launch::dispatch, & do_test_condition_wait_for_pred).join();
    do_test_condition_wait_for_pred();
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* [])
{
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Fiber: condition_variable test suite");

    test->add( BOOST_TEST_CASE( & test_one_waiter_notify_one) );
    test->add( BOOST_TEST_CASE( & test_two_waiter_notify_one) );
    test->add( BOOST_TEST_CASE( & test_two_waiter_notify_all) );
    test->add( BOOST_TEST_CASE( & test_condition_wait) );
    test->add( BOOST_TEST_CASE( & test_condition_wait_until) );
    test->add( BOOST_TEST_CASE( & test_condition_wait_until_pred) );
    test->add( BOOST_TEST_CASE( & test_condition_wait_for) );
    test->add( BOOST_TEST_CASE( & test_condition_wait_for_pred) );

	return test;
}
