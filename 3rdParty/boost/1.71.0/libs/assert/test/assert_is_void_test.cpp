//
//  assert_is_void_test.cpp - tests BOOST_ASSERT_IS_VOID
//
//  Copyright (c) 2015 Ion Gaztanaga
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/config.hpp>

// default case, !NDEBUG
// BOOST_ASSERT(x) -> assert(x)

#undef NDEBUG
#include <boost/assert.hpp>

#ifdef BOOST_ASSERT_IS_VOID
#error "BOOST_ASSERT should NOT be void if NDEBUG is not defined"
#endif

// default case, NDEBUG
// BOOST_ASSERT(x) -> assert(x)

#define NDEBUG
#include <boost/assert.hpp>

#ifndef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should be void in NDEBUG"
#endif

// BOOST_DISABLE_ASSERTS, !NDEBUG
// BOOST_ASSERT(x) -> ((void)0)

#define BOOST_DISABLE_ASSERTS

#undef NDEBUG
#include <boost/assert.hpp>

#ifndef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should be void with BOOST_DISABLE_ASSERTS"
#endif

// BOOST_DISABLE_ASSERTS, NDEBUG
// BOOST_ASSERT(x) -> ((void)0)

#define NDEBUG
#include <boost/assert.hpp>

#ifndef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should be void with BOOST_DISABLE_ASSERTS and NDEBUG"
#endif

#undef BOOST_DISABLE_ASSERTS

// BOOST_ENABLE_ASSERT_HANDLER, !NDEBUG
// BOOST_ASSERT(expr) -> (BOOST_LIKELY(!!(expr))? ((void)0): ::boost::assertion_failed(#expr, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__))

#define BOOST_ENABLE_ASSERT_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>

#ifdef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should NOT be void with BOOST_ENABLE_ASSERT_HANDLER"
#endif

// BOOST_ENABLE_ASSERT_HANDLER, NDEBUG
// BOOST_ASSERT(expr) -> (BOOST_LIKELY(!!(expr))? ((void)0): ::boost::assertion_failed(#expr, BOOST_CURRENT_FUNCTION, __FILE__, __LINE__))

#define NDEBUG
#include <boost/assert.hpp>

#ifdef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should NOT be void with BOOST_ENABLE_ASSERT_HANDLER"
#endif

#undef BOOST_ENABLE_ASSERT_HANDLER

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, !NDEBUG
// same as BOOST_ENABLE_ASSERT_HANDLER

#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER

#undef NDEBUG
#include <boost/assert.hpp>

#ifdef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should NOT be void with BOOST_ENABLE_ASSERT_DEBUG_HANDLER and !NDEBUG"
#endif

// BOOST_ENABLE_ASSERT_DEBUG_HANDLER, NDEBUG
// BOOST_ASSERT(x) -> ((void)0)

#define NDEBUG
#include <boost/assert.hpp>

#ifndef BOOST_ASSERT_IS_VOID
#error "Error: BOOST_ASSERT should be void with BOOST_ENABLE_ASSERT_DEBUG_HANDLER and NDEBUG"
#endif

#undef BOOST_ENABLE_ASSERT_DEBUG_HANDLER

int main()
{
    return 0;
}
