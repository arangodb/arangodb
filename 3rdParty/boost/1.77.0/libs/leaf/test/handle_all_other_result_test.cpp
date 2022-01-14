// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/handle_errors.hpp>
#endif

#include "_test_res.hpp"
#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int> struct info { int value; };

template <class ResType>
ResType f( bool succeed )
{
    if( succeed )
        return 42;
    else
        return make_error_code(std::errc::no_such_file_or_directory);
}

template <class ResType>
ResType g( bool succeed )
{
    if( auto r = f<ResType>(succeed) )
        return r;
    else
        return leaf::error_id(r.error()).load(info<42>{42}).to_error_code();
}

template <class ResType>
void test()
{
    {
        int r = leaf::try_handle_all(
            []
            {
                return g<ResType>(true);
            },
            []
            {
                return -1;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            [&]
            {
                auto r = g<ResType>(false);
                BOOST_TEST(!r);
                auto ec = r.error();
                BOOST_TEST_EQ(ec.message(), "LEAF error");
                BOOST_TEST(!std::strcmp(ec.category().name(),"LEAF error"));
                return r;
            },
            []( info<42> const & x, std::error_code const & ec )
            {
                BOOST_TEST_EQ(x.value, 42);
                BOOST_TEST_EQ(ec, make_error_code(std::errc::no_such_file_or_directory));
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
}

int main()
{
    test<test_res<int, std::error_code>>();
    test<test_res<int const, std::error_code>>();
    test<test_res<int, std::error_code> const>();
    test<test_res<int const, std::error_code> const>();
    return boost::report_errors();
}
