// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#if defined(BOOST_LEAF_NO_EXCEPTIONS) || !defined(BOOST_LEAF_BOOST_AVAILABLE)

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
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/pred.hpp>
#endif

#include "lightweight_test.hpp"
#include <boost/exception/info.hpp>
#include <boost/exception/get_error_info.hpp>

namespace leaf = boost::leaf;

struct test_ex: std::exception { };

typedef boost::error_info<struct test_info_, int> test_info;

int main()
{
    static_assert(std::is_same<test_info, decltype(std::declval<leaf::match<test_info, 42>>().matched)>::value, "handler_argument_traits deduction bug");

    using tr = leaf::leaf_detail::handler_argument_traits<leaf::match<test_info, 42>>;
    static_assert(std::is_same<void, tr::error_type>::value, "handler_argument_traits deduction bug");

    {
        int r = leaf::try_catch(
            []
            {
                try
                {
                    boost::throw_exception(test_ex());
                }
                catch( boost::exception & ex )
                {
                    ex << test_info(42);
                    throw;
                }
                return 0;
            },
            []( test_info x )
            {
                BOOST_TEST_EQ(x.value(), 42);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    {
        int r = leaf::try_catch(
            []
            {
                try
                {
                    boost::throw_exception(test_ex());
                }
                catch( boost::exception & ex )
                {
                    ex << test_info(42);
                    throw;
                }
                return 0;
            },
            []( leaf::match<test_info, 42> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    {
        int r = leaf::try_catch(
            []
            {
                try
                {
                    boost::throw_exception(test_ex());
                }
                catch( boost::exception & ex )
                {
                    ex << test_info(42);
                    throw;
                }
                return 0;
            },
            []( leaf::match<test_info, 41> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    return boost::report_errors();
}

#endif
