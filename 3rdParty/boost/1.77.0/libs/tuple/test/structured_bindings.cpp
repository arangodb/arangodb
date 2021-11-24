// Copyright 2017 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/tuple/tuple.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX17_STRUCTURED_BINDINGS)

BOOST_PRAGMA_MESSAGE("Skipping structured bindings test, not supported")
int main() {}

#else

int main()
{
    // make_tuple

    {
        auto [x1] = boost::make_tuple( 1 );
        BOOST_TEST_EQ( x1, 1 );
    }

    {
        auto [x1, x2] = boost::make_tuple( 1, 2 );
        BOOST_TEST_EQ( x1, 1 );
        BOOST_TEST_EQ( x2, 2 );
    }

    {
        auto [x1, x2, x3] = boost::make_tuple( 1, 2, 3 );
        BOOST_TEST_EQ( x1, 1 );
        BOOST_TEST_EQ( x2, 2 );
        BOOST_TEST_EQ( x3, 3 );
    }

    {
        auto [x1, x2, x3, x4] = boost::make_tuple( 1, 2, 3, 4 );
        BOOST_TEST_EQ( x1, 1 );
        BOOST_TEST_EQ( x2, 2 );
        BOOST_TEST_EQ( x3, 3 );
        BOOST_TEST_EQ( x4, 4 );
    }

    // tuple

    {
        auto [x1] = boost::tuple<int>( 1 );
        BOOST_TEST_EQ( x1, 1 );
    }

    {
        auto [x1, x2] = boost::tuple<int, int>( 1, 2 );
        BOOST_TEST_EQ( x1, 1 );
        BOOST_TEST_EQ( x2, 2 );
    }

    {
        auto [x1, x2, x3] = boost::tuple<int, int, int>( 1, 2, 3 );
        BOOST_TEST_EQ( x1, 1 );
        BOOST_TEST_EQ( x2, 2 );
        BOOST_TEST_EQ( x3, 3 );
    }

    {
        auto [x1, x2, x3, x4] = boost::tuple<int, int, int, int>( 1, 2, 3, 4 );
        BOOST_TEST_EQ( x1, 1 );
        BOOST_TEST_EQ( x2, 2 );
        BOOST_TEST_EQ( x3, 3 );
        BOOST_TEST_EQ( x4, 4 );
    }

    return boost::report_errors();
}

#endif
