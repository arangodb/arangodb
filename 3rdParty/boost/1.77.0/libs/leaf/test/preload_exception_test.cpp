// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#if defined(BOOST_LEAF_NO_EXCEPTIONS) || defined(BOOST_LEAF_NO_THREADS)

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
#   include <boost/leaf/on_error.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int>
struct info
{
    int value;
};

template <class Thrower>
void g1( Thrower th )
{
    auto load = leaf::on_error( info<1>{} );
    th();
}

template <class Thrower>
void g2( Thrower th )
{
    auto load = leaf::on_error(info<3>{}, info<2>{} );
    th();
}

template <class Thrower>
void f1( Thrower th )
{
    return g1(th);
}

template <class Thrower>
void f2( Thrower th )
{
    return g2(th);
}

int main()
{
    BOOST_TEST_EQ(1,
        leaf::try_catch(
            []
            {
                f1( [] { throw leaf::exception(); } );
                return 0;
            },
            []( leaf::error_info const & err, info<1> )
            {
                BOOST_TEST_EQ(err.error().value(), 1);
                return 1;
            },
            []( info<2> )
            {
                return 2;
            },
            []( info<1>, info<2> )
            {
                return 3;
            } ));

    BOOST_TEST_EQ(2,
        leaf::try_catch(
            []
            {
                f2( [] { throw leaf::exception(); } );
                return 0;
            },
            []( info<1> )
            {
                return 1;
            },
            []( leaf::error_info const & err, info<2>, info<3> )
            {
                BOOST_TEST_EQ(err.error().value(), 9);
                return 2;
            },
            []( info<1>, info<2> )
            {
                return 3;
            } ));

    BOOST_TEST_EQ(1,
        leaf::try_catch(
            []
            {
                f1( [] { throw std::exception(); } );
                return 0;
            },
            []( leaf::error_info const & err, info<1> )
            {
                BOOST_TEST_EQ(err.error().value(), 17);
                return 1;
            },
            []( info<2> )
            {
                return 2;
            },
            []( info<1>, info<2> )
            {
                return 3;
            } ) );

    BOOST_TEST_EQ(2,
        leaf::try_catch(
            []
            {
                f2( [] { throw std::exception(); } );
                return 0;
            },
            []( info<1> )
            {
                return 1;
            },
            []( leaf::error_info const & err, info<2>, info<3> )
            {
                BOOST_TEST_EQ(err.error().value(), 21);
                return 2;
            },
            []( info<1>, info<2> )
            {
                return 3;
            } ));

    return boost::report_errors();
}

#endif
