//
//  assert_test2.cpp - a test for BOOST_ASSERT and NDEBUG
//
//  Copyright (c) 2014 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/detail/lightweight_test.hpp>
#include <stdio.h>

// default case, !NDEBUG
// BOOST_ASSERT(x) -> assert(x)

#undef NDEBUG
#include <boost/assert.hpp>

void test_default()
{
    int x = 1;

    BOOST_ASSERT( 1 );
    BOOST_ASSERT( x );
    BOOST_ASSERT( x == 1 );
}

// default case, NDEBUG
// BOOST_ASSERT(x) -> assert(x)

#define NDEBUG
#include <boost/assert.hpp>

void test_default_ndebug()
{
    int x = 1;

    BOOST_ASSERT( 1 );
    BOOST_ASSERT( x );
    BOOST_ASSERT( x == 1 );

    BOOST_ASSERT( 0 );
    BOOST_ASSERT( !x );
    BOOST_ASSERT( x == 0 );

    (void)x;
}

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, !NDEBUG
// same as BOOST_ENABLE_ASSERT_HANDLER

#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>

int handler_invoked = 0;

void boost::assertion_failed( char const * expr, char const * function, char const * file, long line )
{
    printf( "Expression: %s\nFunction: %s\nFile: %s\nLine: %ld\n\n", expr, function, file, line );
    ++handler_invoked;
}

void test_debug_handler()
{
    handler_invoked = 0;

    int x = 1;

    BOOST_ASSERT( 1 );
    BOOST_ASSERT( x );
    BOOST_ASSERT( x == 1 );

    BOOST_ASSERT( 0 );
    BOOST_ASSERT( !x );
    BOOST_ASSERT( x == 0 );

    BOOST_TEST( handler_invoked == 3 );
}

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, NDEBUG
// BOOST_ASSERT(x) -> ((void)0)

#define NDEBUG
#include <boost/assert.hpp>

void test_debug_handler_ndebug()
{
    handler_invoked = 0;

    int x = 1;

    BOOST_ASSERT( 1 );
    BOOST_ASSERT( x );
    BOOST_ASSERT( x == 1 );

    BOOST_ASSERT( 0 );
    BOOST_ASSERT( !x );
    BOOST_ASSERT( x == 0 );

    BOOST_TEST( handler_invoked == 0 );

    (void)x;
}

#undef BOOST_ENABLE_ASSERT_DEBUG_HANDLER

int main()
{
    test_default();
    test_default_ndebug();
    test_debug_handler();
    test_debug_handler_ndebug();

    return boost::report_errors();
}
