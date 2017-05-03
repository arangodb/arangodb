//  (C) Copyright 2008-10 Anthony Williams 
//                2015    Oliver Kowalke
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <boost/test/unit_test.hpp>

#include <boost/fiber/all.hpp>

typedef std::chrono::milliseconds ms;
typedef std::chrono::high_resolution_clock Clock;

int gi = 7;

struct my_exception : public std::runtime_error {
    my_exception() :
        std::runtime_error("my_exception") {
    }
};

struct A {
    A() = default;

    A( A const&) = delete;
    A( A &&) = default;

    A & operator=( A const&) = delete;
    A & operator=( A &&) = default;

    int value;
};

void fn1( boost::fibers::promise< int > * p, int i) {
    boost::this_fiber::yield();
    p->set_value( i);
}

void fn2() {
    boost::fibers::promise< int > p;
    boost::fibers::shared_future< int > f( p.get_future().share() );
    boost::this_fiber::yield();
    boost::fibers::fiber( boost::fibers::launch::post, fn1, & p, 7).detach();
    boost::this_fiber::yield();
    BOOST_CHECK( 7 == f.get() );
}

int fn3() {
    return 3;
}

void fn4() {
}

int fn5() {
    boost::throw_exception( my_exception() );
    return 3;
}

void fn6() {
    boost::throw_exception( my_exception() );
}

int & fn7() {
    return gi;
}

int fn8( int i) {
    return i;
}

A fn9() {
     A a;
     a.value = 3;
     return std::move( a);
}

A fn10() {
    boost::throw_exception( my_exception() );
    return A();
}

void fn11( boost::fibers::promise< int > p) {
  boost::this_fiber::sleep_for( ms(500) );
  p.set_value(3);
}

void fn12( boost::fibers::promise< int& > p) {
  boost::this_fiber::sleep_for( ms(500) );
  gi = 5;
  p.set_value( gi);
}

void fn13( boost::fibers::promise< void > p) {
  boost::this_fiber::sleep_for( ms(500) );
  p.set_value();
}

// shared_future
void test_shared_future_create() {
    {
        // default constructed and assigned shared_future is not valid
        boost::fibers::shared_future< int > f1;
        boost::fibers::shared_future< int > f2 = f1;
        BOOST_CHECK( ! f1.valid() );
        BOOST_CHECK( ! f2.valid() );
    }

    {
        // shared_future retrieved from promise is valid
        boost::fibers::promise< int > p;
        boost::fibers::shared_future< int > f1 = p.get_future();
        boost::fibers::shared_future< int > f2 = f1;
        BOOST_CHECK( f1.valid() );
        BOOST_CHECK( f2.valid() );
    }
}

void test_shared_future_create_ref() {
    {
        // default constructed and assigned shared_future is not valid
        boost::fibers::shared_future< int& > f1;
        boost::fibers::shared_future< int& > f2 = f1;
        BOOST_CHECK( ! f1.valid() );
        BOOST_CHECK( ! f2.valid() );
    }

    {
        // shared_future retrieved from promise is valid
        boost::fibers::promise< int& > p;
        boost::fibers::shared_future< int& > f1 = p.get_future();
        boost::fibers::shared_future< int& > f2 = f1;
        BOOST_CHECK( f1.valid() );
        BOOST_CHECK( f2.valid() );
    }
}

void test_shared_future_create_void() {
    {
        // default constructed and assigned shared_future is not valid
        boost::fibers::shared_future< void > f1;
        boost::fibers::shared_future< void > f2 = f1;
        BOOST_CHECK( ! f1.valid() );
        BOOST_CHECK( ! f2.valid() );
    }

    {
        // shared_future retrieved from promise is valid
        boost::fibers::promise< void > p;
        boost::fibers::shared_future< void > f1 = p.get_future();
        boost::fibers::shared_future< void > f2 = f1;
        BOOST_CHECK( f1.valid() );
        BOOST_CHECK( f2.valid() );
    }
}

void test_shared_future_get() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int > p1;
    p1.set_value( 7);

    boost::fibers::shared_future< int > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // get
    BOOST_CHECK( ! f1.get_exception_ptr() );
    BOOST_CHECK( 7 == f1.get() );
    BOOST_CHECK( f1.valid() );

    // throw broken_promise if promise is destroyed without set
    {
        boost::fibers::promise< int > p2;
        f1 = p2.get_future().share();
    }
    bool thrown = false;
    try {
        f1.get();
    } catch ( boost::fibers::broken_promise const&) {
        thrown = true;
    }
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( thrown);
}

void test_shared_future_get_move() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< A > p1;
    A a; a.value = 7;
    p1.set_value( std::move( a) );

    boost::fibers::shared_future< A > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // get
    BOOST_CHECK( ! f1.get_exception_ptr() );
    BOOST_CHECK( 7 == f1.get().value);
    BOOST_CHECK( f1.valid() );

    // throw broken_promise if promise is destroyed without set
    {
        boost::fibers::promise< A > p2;
        f1 = p2.get_future().share();
    }
    bool thrown = false;
    try {
        f1.get();
    } catch ( boost::fibers::broken_promise const&) {
        thrown = true;
    }
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( thrown);
}

void test_shared_future_get_ref() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int& > p1;
    int i = 7;
    p1.set_value( i);

    boost::fibers::shared_future< int& > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // get
    BOOST_CHECK( ! f1.get_exception_ptr() );
    int & j = f1.get();
    BOOST_CHECK( &i == &j);
    BOOST_CHECK( f1.valid() );

    // throw broken_promise if promise is destroyed without set
    {
        boost::fibers::promise< int& > p2;
        f1 = p2.get_future().share();
    }
    bool thrown = false;
    try {
        f1.get();
    } catch ( boost::fibers::broken_promise const&) {
        thrown = true;
    }
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( thrown);
}


void test_shared_future_get_void() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< void > p1;
    p1.set_value();

    boost::fibers::shared_future< void > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // get
    BOOST_CHECK( ! f1.get_exception_ptr() );
    f1.get();
    BOOST_CHECK( f1.valid() );

    // throw broken_promise if promise is destroyed without set
    {
        boost::fibers::promise< void > p2;
        f1 = p2.get_future().share();
    }
    bool thrown = false;
    try {
        f1.get();
    } catch ( boost::fibers::broken_promise const&) {
        thrown = true;
    }
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( thrown);
}

void test_future_share() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int > p1;
    int i = 7;
    p1.set_value( i);

    boost::fibers::future< int > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // share
    boost::fibers::shared_future< int > sf1 = f1.share();
    BOOST_CHECK( sf1.valid() );
    BOOST_CHECK( ! f1.valid() );

    // get
    BOOST_CHECK( ! sf1.get_exception_ptr() );
    int j = sf1.get();
    BOOST_CHECK_EQUAL( i, j);
    BOOST_CHECK( sf1.valid() );
}

void test_future_share_ref() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int& > p1;
    int i = 7;
    p1.set_value( i);

    boost::fibers::future< int& > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // share
    boost::fibers::shared_future< int& > sf1 = f1.share();
    BOOST_CHECK( sf1.valid() );
    BOOST_CHECK( ! f1.valid() );

    // get
    BOOST_CHECK( ! sf1.get_exception_ptr() );
    int & j = sf1.get();
    BOOST_CHECK( &i == &j);
    BOOST_CHECK( sf1.valid() );
}

void test_future_share_void() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< void > p1;
    p1.set_value();

    boost::fibers::future< void > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // share
    boost::fibers::shared_future< void > sf1 = f1.share();
    BOOST_CHECK( sf1.valid() );
    BOOST_CHECK( ! f1.valid() );

    // get
    BOOST_CHECK( ! sf1.get_exception_ptr() );
    sf1.get();
    BOOST_CHECK( sf1.valid() );
}

void test_shared_future_wait() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int > p1;
    boost::fibers::shared_future< int > f1 = p1.get_future().share();

    // wait on future
    p1.set_value( 7);
    f1.wait();
    BOOST_CHECK( 7 == f1.get() );
}

void test_shared_future_wait_ref() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int& > p1;
    boost::fibers::shared_future< int& > f1 = p1.get_future().share();

    // wait on future
    int i = 7;
    p1.set_value( i);
    f1.wait();
    int & j = f1.get();
    BOOST_CHECK( &i == &j);
}

void test_shared_future_wait_void() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< void > p1;
    boost::fibers::shared_future< void > f1 = p1.get_future().share();

    // wait on future
    p1.set_value();
    f1.wait();
    f1.get();
    BOOST_CHECK( f1.valid() );
}

void test_shared_future_wait_for() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int > p1;
    boost::fibers::shared_future< int > f1 = p1.get_future().share();

    boost::fibers::fiber( boost::fibers::launch::post, fn11, std::move( p1) ).detach();

    // wait on future
    BOOST_CHECK( f1.valid() );
    boost::fibers::future_status status = f1.wait_for( ms(300) );
    BOOST_CHECK( boost::fibers::future_status::timeout == status);

    BOOST_CHECK( f1.valid() );
    status = f1.wait_for( ms(300) );
    BOOST_CHECK( boost::fibers::future_status::ready == status);

    BOOST_CHECK( f1.valid() );
    f1.wait();
}

void test_shared_future_wait_for_ref() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int& > p1;
    boost::fibers::shared_future< int& > f1 = p1.get_future().share();

    boost::fibers::fiber( boost::fibers::launch::post, fn12, std::move( p1) ).detach();

    // wait on future
    BOOST_CHECK( f1.valid() );
    boost::fibers::future_status status = f1.wait_for( ms(300) );
    BOOST_CHECK( boost::fibers::future_status::timeout == status);

    BOOST_CHECK( f1.valid() );
    status = f1.wait_for( ms(300) );
    BOOST_CHECK( boost::fibers::future_status::ready == status);

    BOOST_CHECK( f1.valid() );
    f1.wait();
}

void test_shared_future_wait_for_void() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< void > p1;
    boost::fibers::shared_future< void > f1 = p1.get_future().share();

    boost::fibers::fiber( boost::fibers::launch::post, fn13, std::move( p1) ).detach();

    // wait on future
    BOOST_CHECK( f1.valid() );
    boost::fibers::future_status status = f1.wait_for( ms(300) );
    BOOST_CHECK( boost::fibers::future_status::timeout == status);

    BOOST_CHECK( f1.valid() );
    status = f1.wait_for( ms(300) );
    BOOST_CHECK( boost::fibers::future_status::ready == status);

    BOOST_CHECK( f1.valid() );
    f1.wait();
}

void test_shared_future_wait_until() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int > p1;
    boost::fibers::shared_future< int > f1 = p1.get_future().share();

    boost::fibers::fiber( boost::fibers::launch::post, fn11, std::move( p1) ).detach();

    // wait on future
    BOOST_CHECK( f1.valid() );
    boost::fibers::future_status status = f1.wait_until( Clock::now() + ms(300) );
    BOOST_CHECK( boost::fibers::future_status::timeout == status);

    BOOST_CHECK( f1.valid() );
    status = f1.wait_until( Clock::now() + ms(300) );
    BOOST_CHECK( boost::fibers::future_status::ready == status);

    BOOST_CHECK( f1.valid() );
    f1.wait();
}

void test_shared_future_wait_until_ref() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int& > p1;
    boost::fibers::shared_future< int& > f1 = p1.get_future().share();

    boost::fibers::fiber( boost::fibers::launch::post, fn12, std::move( p1) ).detach();

    // wait on future
    BOOST_CHECK( f1.valid() );
    boost::fibers::future_status status = f1.wait_until( Clock::now() + ms(300) );
    BOOST_CHECK( boost::fibers::future_status::timeout == status);

    BOOST_CHECK( f1.valid() );
    status = f1.wait_until( Clock::now() + ms(300) );
    BOOST_CHECK( boost::fibers::future_status::ready == status);

    BOOST_CHECK( f1.valid() );
    f1.wait();
}

void test_shared_future_wait_until_void() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< void > p1;
    boost::fibers::shared_future< void > f1 = p1.get_future().share();

    boost::fibers::fiber( boost::fibers::launch::post, fn13, std::move( p1) ).detach();

    // wait on future
    BOOST_CHECK( f1.valid() );
    boost::fibers::future_status status = f1.wait_until( Clock::now() + ms(300) );
    BOOST_CHECK( boost::fibers::future_status::timeout == status);

    BOOST_CHECK( f1.valid() );
    status = f1.wait_until( Clock::now() + ms(300) );
    BOOST_CHECK( boost::fibers::future_status::ready == status);

    BOOST_CHECK( f1.valid() );
    f1.wait();
}

void test_shared_future_wait_with_fiber_1() {
    boost::fibers::promise< int > p1;
    boost::fibers::fiber( boost::fibers::launch::post, fn1, & p1, 7).detach();

    boost::fibers::shared_future< int > f1 = p1.get_future().share();

    // wait on future
    BOOST_CHECK( 7 == f1.get() );
}

void test_shared_future_wait_with_fiber_2() {
    boost::fibers::fiber( boost::fibers::launch::post, fn2).join();
}

void test_shared_future_move() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int > p1;
    boost::fibers::shared_future< int > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // move construction
    boost::fibers::shared_future< int > f2( std::move( f1) );
    BOOST_CHECK( ! f1.valid() );
    BOOST_CHECK( f2.valid() );

    // move assignment
    f1 = std::move( f2);
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( ! f2.valid() );
}

void test_shared_future_move_move() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< A > p1;
    boost::fibers::shared_future< A > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // move construction
    boost::fibers::shared_future< A > f2( std::move( f1) );
    BOOST_CHECK( ! f1.valid() );
    BOOST_CHECK( f2.valid() );

    // move assignment
    f1 = std::move( f2);
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( ! f2.valid() );
}

void test_shared_future_move_ref() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< int& > p1;
    boost::fibers::shared_future< int& > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // move construction
    boost::fibers::shared_future< int& > f2( std::move( f1) );
    BOOST_CHECK( ! f1.valid() );
    BOOST_CHECK( f2.valid() );

    // move assignment
    f1 = std::move( f2);
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( ! f2.valid() );
}

void test_shared_future_move_void() {
    // future retrieved from promise is valid (if it is the first)
    boost::fibers::promise< void > p1;
    boost::fibers::shared_future< void > f1 = p1.get_future().share();
    BOOST_CHECK( f1.valid() );

    // move construction
    boost::fibers::shared_future< void > f2( std::move( f1) );
    BOOST_CHECK( ! f1.valid() );
    BOOST_CHECK( f2.valid() );

    // move assignment
    f1 = std::move( f2);
    BOOST_CHECK( f1.valid() );
    BOOST_CHECK( ! f2.valid() );
}


boost::unit_test_framework::test_suite* init_unit_test_suite(int, char*[]) {
    boost::unit_test_framework::test_suite* test =
        BOOST_TEST_SUITE("Boost.Fiber: shared_future test suite");

    test->add(BOOST_TEST_CASE(test_shared_future_create));
    test->add(BOOST_TEST_CASE(test_shared_future_create_ref));
    test->add(BOOST_TEST_CASE(test_shared_future_create_void));
    test->add(BOOST_TEST_CASE(test_shared_future_move));
    test->add(BOOST_TEST_CASE(test_shared_future_move_move));
    test->add(BOOST_TEST_CASE(test_shared_future_move_ref));
    test->add(BOOST_TEST_CASE(test_shared_future_move_void));
    test->add(BOOST_TEST_CASE(test_shared_future_get));
    test->add(BOOST_TEST_CASE(test_shared_future_get_move));
    test->add(BOOST_TEST_CASE(test_shared_future_get_ref));
    test->add(BOOST_TEST_CASE(test_shared_future_get_void));
    test->add(BOOST_TEST_CASE(test_shared_future_wait));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_ref));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_void));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_for));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_for_ref));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_for_void));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_until));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_until_ref));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_until_void));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_with_fiber_1));
    test->add(BOOST_TEST_CASE(test_shared_future_wait_with_fiber_2));

    return test;
}
