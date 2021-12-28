// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/detail/snprintf.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    {
        char buffer[ 64 ];
        boost::system::detail::snprintf( buffer, sizeof(buffer), "...%s...%d...", "xy", 151 );

        BOOST_TEST_CSTR_EQ( buffer, "...xy...151..." );
    }

    {
        char buffer[ 64 ];
        boost::system::detail::snprintf( buffer, sizeof(buffer), "...%s...%d...", "xy", 151 );

        BOOST_TEST_CSTR_EQ( buffer, "...xy...151..." );
    }

    {
        char buffer[ 15 ];
        boost::system::detail::snprintf( buffer, sizeof(buffer), "...%s...%d...", "xy", 151 );

        BOOST_TEST_CSTR_EQ( buffer, "...xy...151..." );
    }

    {
        char buffer[ 14 ];
        boost::system::detail::snprintf( buffer, sizeof(buffer), "...%s...%d...", "xy", 151 );

        BOOST_TEST_CSTR_EQ( buffer, "...xy...151.." );
    }

    {
        char buffer[ 5 ];
        boost::system::detail::snprintf( buffer, sizeof(buffer), "...%s...%d...", "xy", 151 );

        BOOST_TEST_CSTR_EQ( buffer, "...x" );
    }

    {
        char buffer[ 1 ];
        boost::system::detail::snprintf( buffer, sizeof(buffer), "...%s...%d...", "xy", 151 );

        BOOST_TEST_CSTR_EQ( buffer, "" );
    }

    {
        char buffer[ 1 ] = { 'Q' };
        boost::system::detail::snprintf( buffer, 0, "...%s...%d...", "xy", 151 );

        BOOST_TEST_EQ( buffer[0], 'Q' );
    }

    return boost::report_errors();
}
