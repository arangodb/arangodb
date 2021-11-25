//
//  assert_exp_test.cpp - tests BOOST_ASSERT expansion
//
//  Copyright (c) 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/config.hpp>
#include <boost/current_function.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <string>

// Each backslash in __FILE__ when passed through BOOST_STRINGIZE is doubled
static std::string quote( std::string const & s )
{
    std::string r;
    r.reserve( s.size() );

    for( char const * p = s.c_str(); *p; ++p )
    {
        r += *p;
        if( *p == '\\' ) r += *p;
    }

    return r;
}

// default case, !NDEBUG
// BOOST_ASSERT(x) -> assert(x)

#undef NDEBUG
#include <boost/assert.hpp>
#undef assert

void test_default()
{
    std::string v1 = BOOST_STRINGIZE(BOOST_ASSERT(x1));
    BOOST_TEST_EQ( v1, "assert(x1)" );
}

// default case, NDEBUG
// BOOST_ASSERT(x) -> assert(x)

#define NDEBUG
#include <boost/assert.hpp>
#undef assert

void test_default_ndebug()
{
    std::string v2 = BOOST_STRINGIZE(BOOST_ASSERT(x2));
    BOOST_TEST_EQ( v2, "assert(x2)" );
}

// BOOST_DISABLE_ASSERTS, !NDEBUG
// BOOST_ASSERT(x) -> ((void)0)

#define BOOST_DISABLE_ASSERTS

#undef NDEBUG
#include <boost/assert.hpp>

void test_disabled()
{
    std::string v3 = BOOST_STRINGIZE(BOOST_ASSERT(x3));
    BOOST_TEST_EQ( v3, "((void)0)" );
}

// BOOST_DISABLE_ASSERTS, NDEBUG
// BOOST_ASSERT(x) -> ((void)0)

#define NDEBUG
#include <boost/assert.hpp>

void test_disabled_ndebug()
{
    std::string v4 = BOOST_STRINGIZE(BOOST_ASSERT(x4));
    BOOST_TEST_EQ( v4, "((void)0)" );
}

#undef BOOST_DISABLE_ASSERTS

// BOOST_ENABLE_ASSERT_HANDLER, !NDEBUG
// BOOST_ASSERT(expr) -> (BOOST_LIKELY(!!(expr))? ((void)0): ::boost::assertion_failed(#expr, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__))

#undef BOOST_LIKELY
#undef BOOST_CURRENT_FUNCTION

#define BOOST_ENABLE_ASSERT_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>

void test_handler()
{
    std::string v5 = BOOST_STRINGIZE(BOOST_ASSERT(x5)); std::string w5 = "(BOOST_LIKELY(!!(x5))? ((void)0): ::boost::assertion_failed(\"x5\", BOOST_CURRENT_FUNCTION, \"" + quote( __FILE__ ) + "\", " BOOST_STRINGIZE(__LINE__) "))";

    char const * BOOST_CURRENT_FUNCTION = "void test_handler()";
    BOOST_TEST_EQ( v5, w5 );
}

// BOOST_ENABLE_ASSERT_HANDLER, NDEBUG
// BOOST_ASSERT(expr) -> (BOOST_LIKELY(!!(expr))? ((void)0): ::boost::assertion_failed(#expr, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__))

#define NDEBUG
#include <boost/assert.hpp>

void test_handler_ndebug()
{
    std::string v6 = BOOST_STRINGIZE(BOOST_ASSERT(x6)); std::string w6 = "(BOOST_LIKELY(!!(x6))? ((void)0): ::boost::assertion_failed(\"x6\", BOOST_CURRENT_FUNCTION, \"" + quote( __FILE__ ) + "\", " BOOST_STRINGIZE(__LINE__) "))";

    char const * BOOST_CURRENT_FUNCTION = "void test_handler_ndebug()";
    BOOST_TEST_EQ( v6, w6 );
}

#undef BOOST_ENABLE_ASSERT_HANDLER

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, !NDEBUG
// same as BOOST_ENABLE_ASSERT_HANDLER

#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>

void test_debug_handler()
{
    std::string v7 = BOOST_STRINGIZE(BOOST_ASSERT(x7)); std::string w7 = "(BOOST_LIKELY(!!(x7))? ((void)0): ::boost::assertion_failed(\"x7\", BOOST_CURRENT_FUNCTION, \"" + quote( __FILE__ ) + "\", " BOOST_STRINGIZE(__LINE__) "))";

    char const * BOOST_CURRENT_FUNCTION = "void test_debug_handler()";
    BOOST_TEST_EQ( v7, w7 );
}

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, NDEBUG
// BOOST_ASSERT(x) -> ((void)0)

#define NDEBUG
#include <boost/assert.hpp>

void test_debug_handler_ndebug()
{
    std::string v8 = BOOST_STRINGIZE(BOOST_ASSERT(x8));

    char const * BOOST_CURRENT_FUNCTION = "void test_debug_handler_ndebug()";
    BOOST_TEST_EQ( v8, "((void)0)" );
}

#undef BOOST_ENABLE_ASSERT_DEBUG_HANDLER

int main()
{
    test_default();
    test_default_ndebug();
    test_disabled();
    test_disabled_ndebug();
    test_handler();
    test_handler_ndebug();
    test_debug_handler();
    test_debug_handler_ndebug();

    return boost::report_errors();
}
