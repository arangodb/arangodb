// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/capture.hpp>
#   include <boost/leaf/result.hpp>
#   include <boost/leaf/handle_errors.hpp>
#endif

#include "_test_ec.hpp"
#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int> struct info { int value; };

template <class F>
void test( F f )
{
    {
        int c=0;
        auto r = f();
        leaf::try_handle_all(
            [&r]() -> leaf::result<void>
            {
                BOOST_LEAF_CHECK(std::move(r));
                return { };
            },
            [&c]( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    {
        int c=0;
        auto r = f();
        leaf::try_handle_all(
            [&r]() -> leaf::result<void>
            {
                BOOST_LEAF_CHECK(std::move(r));
                return { };
            },
            [&c]( info<2> const & x )
            {
                BOOST_TEST_EQ(x.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    {
        auto r = f();
        int what = leaf::try_handle_all(
            [&r]() -> leaf::result<int>
            {
                BOOST_LEAF_CHECK(std::move(r));
                return 0;
            },
            []( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(what, 1);
    }

    {
        auto r = f();
        int what = leaf::try_handle_all(
            [&r]() -> leaf::result<int>
            {
                BOOST_LEAF_CHECK(std::move(r));
                return 0;
            },
            []( info<2> const & x )
            {
                BOOST_TEST_EQ(x.value, 2);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(what, 2);
    }
}

int main()
{
    test( []
    {
        return leaf::capture(
            std::make_shared<leaf::leaf_detail::polymorphic_context_impl<leaf::context<std::error_code, info<1>, info<2>, info<3>>>>(),
            []() -> leaf::result<int>
            {
                return leaf::new_error(errc_a::a0, info<1>{1}, info<3>{3});
            } );
     } );

    test( []
    {
        return leaf::capture(
            std::make_shared<leaf::leaf_detail::polymorphic_context_impl<leaf::context<std::error_code, info<1>, info<2>, info<3>>>>(),
            []() -> leaf::result<void>
            {
                return leaf::new_error(errc_a::a0, info<1>{1}, info<3>{3});
            } );
     } );

    return boost::report_errors();
}
