//
//  verify_msg_exp_test.cpp - tests BOOST_VERIFY_MSG expansion
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
// BOOST_VERIFY_MSG(x,"m") -> BOOST_ASSERT_MSG(x,"m")

#undef NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT_MSG

void test_default()
{
    std::string v1 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x1, m1));
    BOOST_TEST_EQ( v1, "BOOST_ASSERT_MSG(x1,m1)" );
}

// default case, NDEBUG
// BOOST_VERIFY_MSG(x,"m") -> ((void)(x))

#define NDEBUG
#include <boost/assert.hpp>

void test_default_ndebug()
{
    std::string v2 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x2, m2));
    BOOST_TEST_EQ( v2, "((void)(x2))" );
}

// BOOST_DISABLE_ASSERTS, !NDEBUG
// BOOST_VERIFY_MSG(x,"m") -> ((void)(x))

#define BOOST_DISABLE_ASSERTS

#undef NDEBUG
#include <boost/assert.hpp>

void test_disabled()
{
    std::string v3 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x3, "m3"));
    BOOST_TEST_EQ( v3, "((void)(x3))" );
}

// BOOST_DISABLE_ASSERTS, NDEBUG
// BOOST_VERIFY_MSG(x,"m") -> ((void)(x))

#define NDEBUG
#include <boost/assert.hpp>

void test_disabled_ndebug()
{
    std::string v4 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x4, "m4"));
    BOOST_TEST_EQ( v4, "((void)(x4))" );
}

#undef BOOST_DISABLE_ASSERTS

// BOOST_ENABLE_ASSERT_HANDLER, !NDEBUG
// BOOST_VERIFY_MSG(x,m) -> BOOST_ASSERT_MSG(x,m)

#define BOOST_ENABLE_ASSERT_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT_MSG

void test_handler()
{
    std::string v5 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x5, m5));
    BOOST_TEST_EQ( v5, "BOOST_ASSERT_MSG(x5,m5)" );
}

// BOOST_ENABLE_ASSERT_HANDLER, NDEBUG
// BOOST_VERIFY_MSG(x,n) -> BOOST_ASSERT_MSG(x,m)

#define NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT_MSG

void test_handler_ndebug()
{
    std::string v6 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x6, m6));
    BOOST_TEST_EQ( v6, "BOOST_ASSERT_MSG(x6,m6)" );
}

#undef BOOST_ENABLE_ASSERT_HANDLER

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, !NDEBUG
// BOOST_VERIFY_MSG(x,n) -> BOOST_ASSERT_MSG(x,m)

#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>
#undef BOOST_ASSERT_MSG

void test_debug_handler()
{
    std::string v7 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x7, m7));
    BOOST_TEST_EQ( v7, "BOOST_ASSERT_MSG(x7,m7)" );
}

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, NDEBUG
// BOOST_VERIFY_MSG(x,"m") -> ((void)(x))

#define NDEBUG
#include <boost/assert.hpp>

void test_debug_handler_ndebug()
{
    std::string v8 = BOOST_STRINGIZE(BOOST_VERIFY_MSG(x8, "m8"));
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
