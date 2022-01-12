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
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/exception.hpp>
#   include <boost/leaf/pred.hpp>
#endif

#include "_test_ec.hpp"
#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct info { int value; };

int main()
{
    {
        int r = leaf::try_catch(
            []() -> int
            {
                throw leaf::exception( std::system_error(make_error_code(errc_a::a0)), info{42} );
            },
            []( std::system_error const & se, leaf::match_value<info, 42> )
            {
                BOOST_TEST_EQ(se.code(), errc_a::a0);
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
            []() -> int
            {
                auto load = leaf::on_error(info{42});
                throw std::system_error(make_error_code(errc_a::a0));
            },
            []( std::system_error const & se, leaf::match_value<info, 42> )
            {
                BOOST_TEST_EQ(se.code(), errc_a::a0);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    ////////////////////////////////////////
    {
        int r = leaf::try_catch(
            []() -> int
            {
                throw leaf::exception( std::system_error(make_error_code(errc_a::a0)), info{42} );
            },
            []( leaf::match<leaf::condition<errc_a>, errc_a::a0> code, leaf::match_value<info, 42> )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
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
            []() -> int
            {
                auto load = leaf::on_error(info{42});
                throw std::system_error(make_error_code(errc_a::a0));
            },
            []( leaf::match<leaf::condition<errc_a>, errc_a::a0> code, leaf::match_value<info, 42> )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    ////////////////////////////////////////
    {
        int r = leaf::try_catch(
            []() -> int
            {
                throw leaf::exception( std::system_error(make_error_code(errc_a::a0)), info{42} );
            },
            []( std::error_code const & ec, leaf::match_value<info, 42> )
            {
                BOOST_TEST_EQ(ec, errc_a::a0);
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
            []() -> int
            {
                auto load = leaf::on_error(info{42});
                throw std::system_error(make_error_code(errc_a::a0));
            },
            []( std::error_code const & ec, leaf::match_value<info, 42> )
            {
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    ////////////////////////////////////////
    return boost::report_errors();
}

#endif
