//
//  verify_exp_test.cpp - tests BOOST_ASSERT expansion
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

// default case, !NDEBUG
// BOOST_VERIFY(x) -> BOOST_ASSERT(x)

#undef NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT

void test_default()
{
    std::string v1 = BOOST_STRINGIZE(BOOST_VERIFY(x1));
    BOOST_TEST_EQ( v1, "BOOST_ASSERT(x1)" );
}

// default case, NDEBUG
// BOOST_VERIFY(x) -> ((void)(x))

#define NDEBUG
#include <boost/assert.hpp>

void test_default_ndebug()
{
    std::string v2 = BOOST_STRINGIZE(BOOST_VERIFY(x2));
    BOOST_TEST_EQ( v2, "((void)(x2))" );
}

// BOOST_DISABLE_ASSERTS, !NDEBUG
// BOOST_VERIFY(x) -> ((void)(x))

#define BOOST_DISABLE_ASSERTS
#undef NDEBUG
#include <boost/assert.hpp>

void test_disabled()
{
    std::string v3 = BOOST_STRINGIZE(BOOST_VERIFY(x3));
    BOOST_TEST_EQ( v3, "((void)(x3))" );
}

// BOOST_DISABLE_ASSERTS, NDEBUG
// BOOST_VERIFY(x) -> ((void)(x))

#undef NDEBUG
#include <boost/assert.hpp>

void test_disabled_ndebug()
{
    std::string v4 = BOOST_STRINGIZE(BOOST_VERIFY(x4));
    BOOST_TEST_EQ( v4, "((void)(x4))" );
}

#undef BOOST_DISABLE_ASSERTS

// BOOST_ENABLE_ASSERT_HANDLER, !NDEBUG
// BOOST_VERIFY(x) -> BOOST_ASSERT(x)

#define BOOST_ENABLE_ASSERT_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT

void test_handler()
{
    std::string v5 = BOOST_STRINGIZE(BOOST_VERIFY(x5));
    BOOST_TEST_EQ( v5, "BOOST_ASSERT(x5)" );
}

#define NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT

void test_handler_ndebug()
{
    std::string v6 = BOOST_STRINGIZE(BOOST_VERIFY(x6));
    BOOST_TEST_EQ( v6, "BOOST_ASSERT(x6)" );
}

#undef BOOST_ENABLE_ASSERT_HANDLER

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, !NDEBUG
// BOOST_VERIFY(x) -> BOOST_ASSERT(x)

#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT

void test_debug_handler()
{
    std::string v7 = BOOST_STRINGIZE(BOOST_VERIFY(x7));
    BOOST_TEST_EQ( v7, "BOOST_ASSERT(x7)" );
}

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, NDEBUG
// BOOST_VERIFY(x) -> ((void)(x))

#define NDEBUG
#include <boost/assert.hpp>

void test_debug_handler_ndebug()
{
    std::string v8 = BOOST_STRINGIZE(BOOST_VERIFY(x8));
    BOOST_TEST_EQ( v8, "((void)(x8))" );
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
