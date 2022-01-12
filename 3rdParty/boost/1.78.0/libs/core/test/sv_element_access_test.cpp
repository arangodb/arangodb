// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <stdexcept>

int main()
{
    {
        boost::core::string_view sv( "12345" );

        for( std::size_t i = 0; i < 5; ++i )
        {
            BOOST_TEST_EQ( &sv[ i ], sv.data() + i );
            BOOST_TEST_EQ( &sv.at( i ), sv.data() + i );
        }

        BOOST_TEST_THROWS( sv.at( 5 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.at( boost::core::string_view::npos ), std::out_of_range );
    }

    {
        boost::core::string_view sv;

        BOOST_TEST_THROWS( sv.at( 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.at( 1 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.at( boost::core::string_view::npos ), std::out_of_range );
    }

    {
        boost::core::string_view sv( "12345", 0 );

        BOOST_TEST_THROWS( sv.at( 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.at( 1 ), std::out_of_range );
        BOOST_TEST_THROWS( sv.at( boost::core::string_view::npos ), std::out_of_range );
    }

    return boost::report_errors();
}
