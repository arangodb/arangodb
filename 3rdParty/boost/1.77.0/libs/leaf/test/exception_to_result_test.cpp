// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#ifdef BOOST_LEAF_NO_EXCEPTIONS

#include <iostream>

int main()
{
    std::cout << "Unit test not applicable." << std::endl;
    return 0;
}

#else

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/capture.hpp>
#   include <boost/leaf/result.hpp>
#   include <boost/leaf/handle_errors.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int> struct my_exception: std::exception { };

int main()
{
    {
        int r = leaf::try_handle_all(
            []
            {
                return leaf::exception_to_result<my_exception<1>,my_exception<2>>(
                    []() -> int
                    {
                        throw my_exception<1>();
                    } );
            },
            []( my_exception<1> const &, std::exception_ptr const & ep )
            {
                try
                {
                    std::rethrow_exception(ep);
                }
                catch( my_exception<1> const & )
                {
                }
                return 1;
            },
            []( my_exception<2> const & )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_handle_all(
            []
            {
                return leaf::exception_to_result<my_exception<1>,my_exception<2>>(
                    []() -> int
                    {
                        throw my_exception<2>();
                    } );
            },
            []( my_exception<1> const & )
            {
                return 1;
            },
            []( my_exception<2> const &, std::exception_ptr const & ep )
            {
                try
                {
                    std::rethrow_exception(ep);
                }
                catch( my_exception<2> const & )
                {
                }
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }
    {
        int r = leaf::try_handle_all(
            []
            {
                return leaf::exception_to_result<std::exception,my_exception<1>>(
                    []() -> int
                    {
                        throw my_exception<1>();
                    } );
            },
            []( std::exception const &, std::exception_ptr const & ep )
            {
                try
                {
                    std::rethrow_exception(ep);
                }
                catch( my_exception<1> const & )
                {
                }
                return 1;
            },
            []( my_exception<1> const & )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    return boost::report_errors();
}

#endif
