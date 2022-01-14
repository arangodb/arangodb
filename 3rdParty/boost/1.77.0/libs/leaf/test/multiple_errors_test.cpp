// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#include <boost/leaf/handle_errors.hpp>

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"
#include <exception>

namespace leaf = boost::leaf;

template <int A>
struct info
{
    int value;
};

leaf::result<void> f12()
{
    return leaf::new_error( info<1>{1}, info<2>{2} );
}

leaf::result<void> f23()
{
    return leaf::new_error( info<2>{2}, info<3>{3} );
}

int main()
{
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                leaf::result<void> r1 = f12();
                (void) r1;
                leaf::result<void> r2 = f23();
                return r2.error();
            },
            []( info<1> )
            {
                return 1;
            },
            []( info<2> const & x, info<3> const & y )
            {
                BOOST_TEST_EQ(x.value, 2);
                BOOST_TEST_EQ(y.value, 3);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

#ifndef BOOST_LEAF_NO_EXCEPTIONS
    {
        int r = leaf::try_catch(
            []() -> int
            {
                try
                {
                    throw leaf::exception(info<4>{4});
                }
                catch(...)
                {
                }
                throw std::exception{};
            },
            []( std::exception const &, info<4> )
            {
                return 1;
            },
            []( std::exception const & )
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }
#endif

    return boost::report_errors();
}
