// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#define BOOST_GEOMETRY_ENABLE_ASSERT_HANDLER
#include <boost/geometry/core/assert.hpp>

struct assert_failure_exception
    : std::exception
{
    const char * what() const throw()
    {
        return "assertion failure";
    }
};

namespace boost { namespace geometry {

inline void assertion_failed(char const * expr, char const * function, char const * file, long line)
{
    throw assert_failure_exception();
}

inline void assertion_failed_msg(char const * expr, char const * msg, char const * function, char const * file, long line)
{
    throw assert_failure_exception();
}

}}

void fun1(bool condition)
{
    BOOST_GEOMETRY_ASSERT(condition);
}

void fun2(bool condition, const char* msg = "")
{
    BOOST_GEOMETRY_ASSERT_MSG(condition, msg);
}

bool is_ok(assert_failure_exception const& ) { return true; }

int test_main(int, char* [])
{
    int a = 1;

    fun1(a == 1);
    BOOST_CHECK_EXCEPTION(fun1(a == 2), assert_failure_exception, is_ok);
    fun2(a == 1);
    BOOST_CHECK_EXCEPTION(fun2(a == 2), assert_failure_exception, is_ok);

    return 0;
}
