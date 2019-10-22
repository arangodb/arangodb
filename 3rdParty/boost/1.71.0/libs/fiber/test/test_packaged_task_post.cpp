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

struct B {
    bool    bset{ false };

    B() = default;

    B( bool set) :
        bset{ set } {
        gi = 3;
    }

    ~B() {
        if ( bset) {
            gi = -1;
        }
    }

    B( B && other) :
        bset{ other.bset } {
        other.bset = false;
    }

    B & operator=( B && other) {
        if ( this == & other) return * this;
        bset = other.bset;
        other.bset = false;
        return * this;
    }

    B( B const&) = delete;
    B & operator=( B const&) = delete;
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
     return a;
}

A fn10() {
    boost::throw_exception( my_exception() );
    return A();
}

B fn11( bool set) {
     B b( set);
     return b;
}

// packaged_task
void test_packaged_task_create() {
    // default constructed packaged_task is not valid
    boost::fibers::packaged_task< int() > t1;
    BOOST_CHECK( ! t1.valid() );

    // packaged_task from function
    boost::fibers::packaged_task< int() > t2( fn3);
    BOOST_CHECK( t2.valid() );
}

// packaged_task
void test_packaged_task_create_move() {
    // default constructed packaged_task is not valid
    boost::fibers::packaged_task< A() > t1;
    BOOST_CHECK( ! t1.valid() );

    // packaged_task from function
    boost::fibers::packaged_task< A() > t2( fn9);
    BOOST_CHECK( t2.valid() );
}

void test_packaged_task_create_void() {
    // default constructed packaged_task is not valid
    boost::fibers::packaged_task< void() > t1;
    BOOST_CHECK( ! t1.valid() );

    // packaged_task from function
    boost::fibers::packaged_task< void() > t2( fn4);
    BOOST_CHECK( t2.valid() );
}

void test_packaged_task_move() {
    boost::fibers::packaged_task< int() > t1( fn3);
    BOOST_CHECK( t1.valid() );

    // move construction
    boost::fibers::packaged_task< int() > t2( std::move( t1) );
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );

    // move assignment
    t1 = std::move( t2);
    BOOST_CHECK( t1.valid() );
    BOOST_CHECK( ! t2.valid() );
}

void test_packaged_task_move_move() {
    boost::fibers::packaged_task< A() > t1( fn9);
    BOOST_CHECK( t1.valid() );

    // move construction
    boost::fibers::packaged_task< A() > t2( std::move( t1) );
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );

    // move assignment
    t1 = std::move( t2);
    BOOST_CHECK( t1.valid() );
    BOOST_CHECK( ! t2.valid() );
}

void test_packaged_task_move_void() {
    boost::fibers::packaged_task< void() > t1( fn4);
    BOOST_CHECK( t1.valid() );

    // move construction
    boost::fibers::packaged_task< void() > t2( std::move( t1) );
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );

    // move assignment
    t1 = std::move( t2);
    BOOST_CHECK( t1.valid() );
    BOOST_CHECK( ! t2.valid() );
}

void test_packaged_task_swap() {
    boost::fibers::packaged_task< int() > t1( fn3);
    BOOST_CHECK( t1.valid() );

    boost::fibers::packaged_task< int() > t2;
    BOOST_CHECK( ! t2.valid() );

    // swap
    t1.swap( t2);
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );
}

void test_packaged_task_swap_move() {
    boost::fibers::packaged_task< A() > t1( fn9);
    BOOST_CHECK( t1.valid() );

    boost::fibers::packaged_task< A() > t2;
    BOOST_CHECK( ! t2.valid() );

    // swap
    t1.swap( t2);
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );
}

void test_packaged_task_swap_void() {
    boost::fibers::packaged_task< void() > t1( fn4);
    BOOST_CHECK( t1.valid() );

    boost::fibers::packaged_task< void() > t2;
    BOOST_CHECK( ! t2.valid() );

    // swap
    t1.swap( t2);
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );
}

void test_packaged_task_reset() {
    {
        boost::fibers::packaged_task< int() > p( fn3);
        boost::fibers::future< int > f( p.get_future() );
        BOOST_CHECK( p.valid() );

        p();
        BOOST_CHECK( 3 == f.get() );

        // reset
        p.reset();
        p();
        f = p.get_future();
        BOOST_CHECK( 3 == f.get() );
    }
    {
        boost::fibers::packaged_task< int() > p;

        bool thrown = false;
        try {
            p.reset();
        } catch ( boost::fibers::packaged_task_uninitialized const&) {
            thrown = true;
        }
        BOOST_CHECK( thrown);
    }
}

void test_packaged_task_reset_destruction() {
        gi = 0;
        boost::fibers::packaged_task< B( bool) > p( fn11);
        BOOST_CHECK( p.valid() );

        BOOST_CHECK( 0 == gi);
        p( true);
        BOOST_CHECK( 3 == gi);

        // reset
        p.reset();
        BOOST_CHECK( -1 == gi);
        p( false);
        BOOST_CHECK( 3 == gi);

        // reset
        p.reset();
        BOOST_CHECK( 3 == gi);
}

void test_packaged_task_reset_move() {
    {
        boost::fibers::packaged_task< A() > p( fn9);
        boost::fibers::future< A > f( p.get_future() );
        BOOST_CHECK( p.valid() );

        p();
        BOOST_CHECK( 3 == f.get().value);

        // reset
        p.reset();
        p();
        f = p.get_future();
        BOOST_CHECK( 3 == f.get().value);
    }
    {
        boost::fibers::packaged_task< A() > p;

        bool thrown = false;
        try {
            p.reset();
        } catch ( boost::fibers::packaged_task_uninitialized const&) {
            thrown = true;
        }
        BOOST_CHECK( thrown);
    }
}

void test_packaged_task_reset_void() {
    {
        boost::fibers::packaged_task< void() > p( fn4);
        boost::fibers::future< void > f( p.get_future() );
        BOOST_CHECK( p.valid() );

        p();
        f.get();

        // reset
        p.reset();
        p();
        f = p.get_future();
        f.get();
    }
    {
        boost::fibers::packaged_task< void() > p;

        bool thrown = false;
        try {
            p.reset();
        } catch ( boost::fibers::packaged_task_uninitialized const&) {
            thrown = true;
        }
        BOOST_CHECK( thrown);
    }
}

void test_packaged_task_get_future() {
    boost::fibers::packaged_task< int() > t1( fn3);
    BOOST_CHECK( t1.valid() );

    // retrieve future
    boost::fibers::future< int > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // retrieve future a second time
    bool thrown = false;
    try {
        f1 = t1.get_future();
    } catch ( boost::fibers::future_already_retrieved const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // move construction
    boost::fibers::packaged_task< int() > t2( std::move( t1) );
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );

    // retrieve future from uninitialized
    thrown = false;
    try {
        f1 = t1.get_future();
    } catch ( boost::fibers::packaged_task_uninitialized const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_packaged_task_get_future_move() {
    boost::fibers::packaged_task< A() > t1( fn9);
    BOOST_CHECK( t1.valid() );

    // retrieve future
    boost::fibers::future< A > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // retrieve future a second time
    bool thrown = false;
    try {
        f1 = t1.get_future();
    } catch ( boost::fibers::future_already_retrieved const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // move construction
    boost::fibers::packaged_task< A() > t2( std::move( t1) );
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );

    // retrieve future from uninitialized
    thrown = false;
    try {
        f1 = t1.get_future();
    } catch ( boost::fibers::packaged_task_uninitialized const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_packaged_task_get_future_void() {
    boost::fibers::packaged_task< void() > t1( fn4);
    BOOST_CHECK( t1.valid() );

    // retrieve future
    boost::fibers::future< void > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // retrieve future a second time
    bool thrown = false;
    try {
        f1 = t1.get_future();
    } catch ( boost::fibers::future_already_retrieved const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    // move construction
    boost::fibers::packaged_task< void() > t2( std::move( t1) );
    BOOST_CHECK( ! t1.valid() );
    BOOST_CHECK( t2.valid() );

    // retrieve future from uninitialized
    thrown = false;
    try {
        f1 = t1.get_future();
    } catch ( boost::fibers::packaged_task_uninitialized const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_packaged_task_exec() {
    // promise takes a copyable as return type
    boost::fibers::packaged_task< int() > t1( fn3);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< int > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // exec
    t1();
    BOOST_CHECK( 3 == f1.get() );

    // exec a second time
    bool thrown = false;
    try {
        t1();
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_packaged_task_exec_move() {
    // promise takes a copyable as return type
    boost::fibers::packaged_task< A() > t1( fn9);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< A > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // exec
    t1();
    BOOST_CHECK( 3 == f1.get().value);

    // exec a second time
    bool thrown = false;
    try {
        t1();
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_packaged_task_exec_param() {
    // promise takes a copyable as return type
    boost::fibers::packaged_task< int( int) > t1( fn8);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< int > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // exec
    t1( 3);
    BOOST_CHECK( 3 == f1.get() );

    // exec a second time
    bool thrown = false;
    try {
        t1( 7);
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    //TODO: packaged_task returns a moveable-only as return type
}

void test_packaged_task_exec_ref() {
    // promise takes a copyable as return type
    boost::fibers::packaged_task< int&() > t1( fn7);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< int& > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // exec
    t1();
    int & i = f1.get();
    BOOST_CHECK( &gi == &i);

    // exec a second time
    bool thrown = false;
    try {
        t1();
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    //TODO: packaged_task returns a moveable-only as return type
}

void test_packaged_task_exec_void() {
    // promise takes a copyable as return type
    boost::fibers::packaged_task< void() > t1( fn4);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< void > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // set void
    t1();
    f1.get();

    // exec a second time
    bool thrown = false;
    try {
        t1();
    } catch ( boost::fibers::promise_already_satisfied const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}

void test_packaged_task_exception() {
    // promise takes a copyable as return type
    boost::fibers::packaged_task< int() > t1( fn5);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< int > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // exec
    t1();
    bool thrown = false;
    try {
        f1.get();
    } catch ( my_exception const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    boost::fibers::packaged_task< int() > t2( fn5);
    BOOST_CHECK( t2.valid() );
    boost::fibers::future< int > f2 = t2.get_future();
    BOOST_CHECK( f2.valid() );

    // exec
    t2();
    BOOST_CHECK( f2.get_exception_ptr() );
    thrown = false;
    try
    { std::rethrow_exception( f2.get_exception_ptr() ); }
    catch ( my_exception const&)
    { thrown = true; }
    BOOST_CHECK( thrown);
}

void test_packaged_task_exception_move() {
    // promise takes a moveable as return type
    boost::fibers::packaged_task< A() > t1( fn10);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< A > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // exec
    t1();
    bool thrown = false;
    try {
        f1.get();
    } catch ( my_exception const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);

    boost::fibers::packaged_task< A() > t2( fn10);
    BOOST_CHECK( t2.valid() );
    boost::fibers::future< A > f2 = t2.get_future();
    BOOST_CHECK( f2.valid() );

    // exec
    t2();
    BOOST_CHECK( f2.get_exception_ptr() );
    thrown = false;
    try
    { std::rethrow_exception( f2.get_exception_ptr() ); }
    catch ( my_exception const&)
    { thrown = true; }
    BOOST_CHECK( thrown);
}

void test_packaged_task_exception_void() {
    // promise takes a copyable as return type
    boost::fibers::packaged_task< void() > t1( fn6);
    BOOST_CHECK( t1.valid() );
    boost::fibers::future< void > f1 = t1.get_future();
    BOOST_CHECK( f1.valid() );

    // set void
    t1();
    bool thrown = false;
    try {
        f1.get();
    } catch ( my_exception const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
    
    boost::fibers::packaged_task< void() > t2( fn6);
    BOOST_CHECK( t2.valid() );
    boost::fibers::future< void > f2 = t2.get_future();
    BOOST_CHECK( f2.valid() );

    // exec
    t2();
    BOOST_CHECK( f2.get_exception_ptr() );
    thrown = false;
    try {
        std::rethrow_exception( f2.get_exception_ptr() );
    } catch ( my_exception const&) {
        thrown = true;
    }
    BOOST_CHECK( thrown);
}


boost::unit_test_framework::test_suite* init_unit_test_suite(int, char*[]) {
    boost::unit_test_framework::test_suite* test =
        BOOST_TEST_SUITE("Boost.Fiber: packaged_task test suite");

    test->add(BOOST_TEST_CASE(test_packaged_task_create));
    test->add(BOOST_TEST_CASE(test_packaged_task_create_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_create_void));
    test->add(BOOST_TEST_CASE(test_packaged_task_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_move_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_move_void));
    test->add(BOOST_TEST_CASE(test_packaged_task_swap));
    test->add(BOOST_TEST_CASE(test_packaged_task_swap_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_swap_void));
    test->add(BOOST_TEST_CASE(test_packaged_task_reset));
    test->add(BOOST_TEST_CASE(test_packaged_task_reset_destruction));
    test->add(BOOST_TEST_CASE(test_packaged_task_reset_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_reset_void));
    test->add(BOOST_TEST_CASE(test_packaged_task_get_future));
    test->add(BOOST_TEST_CASE(test_packaged_task_get_future_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_get_future_void));
    test->add(BOOST_TEST_CASE(test_packaged_task_exec));
    test->add(BOOST_TEST_CASE(test_packaged_task_exec_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_exec_param));
    test->add(BOOST_TEST_CASE(test_packaged_task_exec_ref));
    test->add(BOOST_TEST_CASE(test_packaged_task_exec_void));
    test->add(BOOST_TEST_CASE(test_packaged_task_exception));
    test->add(BOOST_TEST_CASE(test_packaged_task_exception_move));
    test->add(BOOST_TEST_CASE(test_packaged_task_exception_void));

    return test;
}
