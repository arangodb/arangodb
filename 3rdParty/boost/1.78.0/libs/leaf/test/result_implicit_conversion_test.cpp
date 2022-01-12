// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct A
{
    int x;
    A() noexcept:
        x(0)
    {
    }

    A( int x ) noexcept:
        x(x)
    {
    }
};

leaf::result<int> f()
{
    return 42;
}

leaf::result<A> g()
{
    return f();
}

int main()
{
    BOOST_TEST_EQ(g().value().x, 42);
    {
        leaf::result<int> r1(42);
        leaf::result<A> r2(std::move(r1));
        BOOST_TEST_EQ(r2.value().x, 42);
    }
    {
        leaf::result<int> r1(42);
        leaf::result<A> r2;
        r2 = std::move(r1);
        BOOST_TEST_EQ(r2.value().x, 42);
    }
    return boost::report_errors();
}
