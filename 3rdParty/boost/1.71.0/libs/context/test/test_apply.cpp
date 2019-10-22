
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/context/detail/apply.hpp>
#include <boost/context/detail/config.hpp>

namespace ctx = boost::context;

struct callable {
    int k{ 0 };

    callable() = default;

    callable( int k_) :
        k{ k_ } {
    }

    int foo( int i, int j) const {
        return i + j + k;
    }

    int operator()( int i, int j) const {
        return foo( i, j);
    }
};

struct movable {
    int k{ 0 };

    movable() = default;

    movable( int k_) :
        k{ k_ } {
    }

    movable( movable const&) = delete;
    movable & operator=( movable const&) = delete;

    movable( movable && other) :
        k{ other.k } {
        other.k = -1;
    }

    movable & operator=( movable && other) {
        if ( this == & other) return * this;
        k = other.k;
        other.k = -1;
        return * this;
    }

    int foo( int i, int j) const {
        return i + j + k;
    }

    int operator()( int i, int j) const {
        return foo( i, j);
    }
};

int fn1( int i, int j) {
    return i + j;
}

int * fn2( int * ip) {
    return ip;
}

int * fn3( int & ir) {
    return & ir;
}

int & fn4( int & ir) {
    return ir;
}

int fn5( int i, callable && c) {
    return i + c.k;
}

int fn6( int i, movable && m) {
    return i + m.k;
}

void test1() {
    int result = ctx::detail::apply( fn1, std::make_tuple( 1, 2) );
    BOOST_CHECK_EQUAL( result, 3);
}

void test2() {
    {
        int i = 3;
        int * ip = & i;
        int * result = ctx::detail::apply( fn2, std::make_tuple( ip) );
        BOOST_CHECK_EQUAL( result, ip);
        BOOST_CHECK_EQUAL( * result, i);
    }
    {
        int i = 3;
        int * result = ctx::detail::apply( fn2, std::make_tuple( & i) );
        BOOST_CHECK_EQUAL( result, & i);
        BOOST_CHECK_EQUAL( * result, i);
    }
}

void test3() {
    {
        int i = 'c';
        int & ir = i;
        int * result = ctx::detail::apply( fn3, std::make_tuple( std::ref( ir) ) );
        BOOST_CHECK_EQUAL( result, & ir);
        BOOST_CHECK_EQUAL( * result, i);
    }
    {
        int i = 'c';
        int * result = ctx::detail::apply( fn3, std::make_tuple( std::ref( i) ) );
        BOOST_CHECK_EQUAL( result, & i);
        BOOST_CHECK_EQUAL( * result, i);
    }
}

void test4() {
    {
        int i = 3;
        int & ir = i;
        int & result = ctx::detail::apply( fn4, std::make_tuple( std::ref( ir) ) );
        BOOST_CHECK_EQUAL( result, ir);
        BOOST_CHECK_EQUAL( & result, & ir);
        BOOST_CHECK_EQUAL( result, i);
    }
    {
        int i = 3;
        int & result = ctx::detail::apply( fn4, std::make_tuple( std::ref( i) ) );
        BOOST_CHECK_EQUAL( & result, & i);
        BOOST_CHECK_EQUAL( result, i);
    }
}

void test5() {
    {
        callable c( 5);
        int result = ctx::detail::apply( fn5, std::make_tuple( 1, std::move( c) ) );
        BOOST_CHECK_EQUAL( result, 6);
        BOOST_CHECK_EQUAL( c.k, 5);
    }
    {
        movable m( 5);
        int result = ctx::detail::apply( fn6, std::make_tuple( 1, std::move( m) ) );
        BOOST_CHECK_EQUAL( result, 6);
        BOOST_CHECK_EQUAL( m.k, -1);
    }
}

void test6() {
    {
        callable c;
        int result = ctx::detail::apply( c, std::make_tuple( 1, 2) );
        BOOST_CHECK_EQUAL( result, 3);
        BOOST_CHECK_EQUAL( c.k, 0);
    }
    {
        callable c;
        int result = ctx::detail::apply( & callable::foo, std::make_tuple( c, 1, 2) );
        BOOST_CHECK_EQUAL( result, 3);
        BOOST_CHECK_EQUAL( c.k, 0);
    }
}

void test7() {
    {
        movable m;
        int result = ctx::detail::apply( std::move( m), std::make_tuple( 1, 2) );
        BOOST_CHECK_EQUAL( result, 3);
    }
    {
        movable m;
        int result = ctx::detail::apply( & movable::foo, std::make_tuple( std::move( m), 1, 2) );
        BOOST_CHECK_EQUAL( result, 3);
    }
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* [])
{
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Context: apply test suite");

    test->add( BOOST_TEST_CASE( & test1) );
    test->add( BOOST_TEST_CASE( & test2) );
    test->add( BOOST_TEST_CASE( & test3) );
    test->add( BOOST_TEST_CASE( & test4) );
    test->add( BOOST_TEST_CASE( & test5) );
    test->add( BOOST_TEST_CASE( & test6) );
    test->add( BOOST_TEST_CASE( & test7) );

    return test;
}
