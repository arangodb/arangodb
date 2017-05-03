//  (C) Copyright 2008-10 Anthony Williams 
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <utility>
#include <memory>
#include <stdexcept>
#include <string>

#include <boost/test/unit_test.hpp>

#include <boost/fiber/all.hpp>

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
    boost::fibers::future< int > f( p.get_future() );
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

// promise
void test_promise_create() {
    // use std::allocator<> as default
    boost::fibers::promise< int > p1;

    // use std::allocator<> as user defined
    std::allocator< boost::fibers::promise< int > > alloc;
    boost::fibers::promise< int > p2( std::allocator_arg,  alloc);
}

void test_promise_create_ref() {
    // use std::allocator<> as default
    boost::fibers::promise< int& > p1;

    // use std::allocator<> as user defined
    std::allocator< boost::fibers::promise< int& > > alloc;
    boost::fibers::promise< int& > p2( std::allocator_arg, alloc);
}

void test_promise_create_void() {
    // use std::allocator<> as default
    boost::fibers::promise< void > p1;

    // use std::allocator<> as user defined
    std::allocator< boost::fibers::promise< void > > alloc;
    boost::fibers::promise< void > p2( std::allocator_arg, alloc);
}

void test_promise_move() {
    boost::fibers::promise< int > p1;

    // move construction
    boost::fibers::promise< int > p2( std::move( p1) );

    // move assigment
    p1 = std::move( p2);
}

void test_promise_move_ref() {
    boost::fibers::promise< int& > p1;

    // move construction
    boost::fibers::promise< int& > p2( std::move( p1) );

    // move assigment
    p1 = std::move( p2);
}

void test_promise_move_void() {
    boost::fibers::promise< void > p1;

    // move construction
    boost::fibers::promise< void > p2( std::move( p1) );

    // move assigment
    p1 = std::move( p2);
}

void test_promise_swap() {
    boost::fibers::promise< int > p1;

    // move construction
    boost::fibers::promise< int > p2( std::move( p1) );

    // swap
    p1.swap( p2);
}

void test_promise_swap_ref() {
    boost::fibers::promise< int& > p1;

    // move construction
    boost::fibers::promise< int& > p2( std::move( p1) );

    // swap
    p1.swap( p2);
}

void test_promise_swap_void() {
    boost::fibers::promise< void > p1;

    // move construction
    boost::fibers::promise< void > p2( std::move( p1) );

    // swap
    p1.swap( p2);
}

void test_promise_get_future() {
    boost::fibers::promise< int > p1;

    // retrieve future
    boost::fibers::future< int > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // retrieve future a second time
    bool thrown = false;
    try {
        f1 = p1.get_future();
    } catch ( boost::fibers::future_already_retrieved const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // move construction
    boost::fibers::promise< int > p2( std::move( p1) );

    // retrieve future from uninitialized
    thrown = false;
    try {
        f1 = p1.get_future();
    } catch ( boost::fibers::promise_uninitialized const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_get_future_ref() {
    boost::fibers::promise< int& > p1;

    // retrieve future
    boost::fibers::future< int& > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // retrieve future a second time
    bool thrown = false;
    try {
        f1 = p1.get_future();
    } catch ( boost::fibers::future_already_retrieved const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // move construction
    boost::fibers::promise< int& > p2( std::move( p1) );

    // retrieve future from uninitialized
    thrown = false;
    try {
        f1 = p1.get_future();
    } catch ( boost::fibers::promise_uninitialized const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_get_future_void() {
    boost::fibers::promise< void > p1;

    // retrieve future
    boost::fibers::future< void > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // retrieve future a second time
    bool thrown = false;
    try {
        f1 = p1.get_future();
    } catch ( boost::fibers::future_already_retrieved const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // move construction
    boost::fibers::promise< void > p2( std::move( p1) );

    // retrieve future from uninitialized
    thrown = false;
    try {
        f1 = p1.get_future();
    } catch ( boost::fibers::promise_uninitialized const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_set_value() {
    // promise takes a copyable as return type
    boost::fibers::promise< int > p1;
    boost::fibers::future< int > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // copy value
    p1.set_value( 7);
    BOOST_CHECK( 7 == f1.get() );

    // set value a second time
    bool thrown = false;
    try {
        p1.set_value( 11);
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_set_value_move() {
    // promise takes a copyable as return type
    boost::fibers::promise< A > p1;
    boost::fibers::future< A > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // move value
    A a1; a1.value = 7;
    p1.set_value( std::move( a1) );
    A a2 = f1.get();
    BOOST_CHECK( 7 == a2.value);

    // set value a second time
    bool thrown = false;
    try {
        A a;
        p1.set_value( std::move( a) );
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_set_value_ref() {
    // promise takes a reference as return type
    boost::fibers::promise< int& > p1;
    boost::fibers::future< int& > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // copy value
    int i = 7;
    p1.set_value( i);
    int & j = f1.get();
    BOOST_CHECK( &i == &j);

    // set value a second time
    bool thrown = false;
    try {
        p1.set_value( i);
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_set_value_void() {
    // promise takes a copyable as return type
    boost::fibers::promise< void > p1;
    boost::fibers::future< void > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );

    // set void
    p1.set_value();
    f1.get();

    // set value a second time
    bool thrown = false;
    try {
        p1.set_value();
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_set_exception() {
    boost::fibers::promise< int > p1;
    boost::fibers::future< int > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );
    p1.set_exception( std::make_exception_ptr( my_exception() ) );

    // set exception a second time
    bool thrown = false;
    try {
        p1.set_exception( std::make_exception_ptr( my_exception() ) );
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // set value
    thrown = false;
    try
    { p1.set_value( 11); }
    catch ( boost::fibers::promise_already_satisfied const&)
    { thrown = true; }
    BOOST_CHECK( thrown);
}

void test_promise_set_exception_ref() {
    boost::fibers::promise< int& > p1;
    boost::fibers::future< int& > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );
    p1.set_exception( std::make_exception_ptr( my_exception() ) );

    // set exception a second time
    bool thrown = false;
    try {
        p1.set_exception( std::make_exception_ptr( my_exception() ) );
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // set value
    thrown = false;
    int i = 11;
    try {
        p1.set_value( i);
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_promise_set_exception_void() {
    boost::fibers::promise< void > p1;
    boost::fibers::future< void > f1 = p1.get_future();
    BOOST_CHECK( f1.valid() );
    p1.set_exception( std::make_exception_ptr( my_exception() ) );

    // set exception a second time
    bool thrown = false;
    try {
        p1.set_exception( std::make_exception_ptr( my_exception() ) );
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // set value
    thrown = false;
    try {
        p1.set_value();
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

boost::unit_test_framework::test_suite* init_unit_test_suite(int, char*[]) {
    boost::unit_test_framework::test_suite* test =
        BOOST_TEST_SUITE("Boost.Fiber: promise test suite");

    test->add(BOOST_TEST_CASE(test_promise_create));
    test->add(BOOST_TEST_CASE(test_promise_create_ref));
    test->add(BOOST_TEST_CASE(test_promise_create_void));
    test->add(BOOST_TEST_CASE(test_promise_move));
    test->add(BOOST_TEST_CASE(test_promise_move_ref));
    test->add(BOOST_TEST_CASE(test_promise_move_void));
    test->add(BOOST_TEST_CASE(test_promise_swap));
    test->add(BOOST_TEST_CASE(test_promise_swap_ref));
    test->add(BOOST_TEST_CASE(test_promise_swap_void));
    test->add(BOOST_TEST_CASE(test_promise_get_future));
    test->add(BOOST_TEST_CASE(test_promise_get_future_ref));
    test->add(BOOST_TEST_CASE(test_promise_get_future_void));
    test->add(BOOST_TEST_CASE(test_promise_set_value));
    test->add(BOOST_TEST_CASE(test_promise_set_value_move));
    test->add(BOOST_TEST_CASE(test_promise_set_value_ref));
    test->add(BOOST_TEST_CASE(test_promise_set_value_void));
    test->add(BOOST_TEST_CASE(test_promise_set_exception));
    test->add(BOOST_TEST_CASE(test_promise_set_exception_ref));
    test->add(BOOST_TEST_CASE(test_promise_set_exception_void));

    return test;
}
