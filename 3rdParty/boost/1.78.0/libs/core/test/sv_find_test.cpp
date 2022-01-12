// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

int main()
{
    std::size_t const npos = boost::core::string_view::npos;

    {
        boost::core::string_view sv( "" );

        BOOST_TEST_EQ( sv.find( boost::core::string_view() ), 0 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view(), 1 ), npos );

        BOOST_TEST_EQ( sv.find( boost::core::string_view( "" ) ), 0 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "" ), 1 ), npos );

        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ) ), npos );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ), 1 ), npos );

        BOOST_TEST_EQ( sv.find( '1' ), npos );
        BOOST_TEST_EQ( sv.find( '1', 1 ), npos );

        BOOST_TEST_EQ( sv.find( "" ), 0 );
        BOOST_TEST_EQ( sv.find( "", 1 ), npos );

        BOOST_TEST_EQ( sv.find( "1" ), npos );
        BOOST_TEST_EQ( sv.find( "1", 1 ), npos );

        BOOST_TEST_EQ( sv.find( "1", 0, 0 ), 0 );
        BOOST_TEST_EQ( sv.find( "1", 1, 0 ), npos );

        BOOST_TEST_EQ( sv.find( "1", 0, 1 ), npos );
        BOOST_TEST_EQ( sv.find( "1", 1, 1 ), npos );
    }

    {
        boost::core::string_view sv( "123123" );

        BOOST_TEST_EQ( sv.find( boost::core::string_view() ), 0 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view(), 1 ), 1 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view(), 6 ), 6 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view(), 7 ), npos );

        BOOST_TEST_EQ( sv.find( boost::core::string_view( "" ) ), 0 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "" ), 6 ), 6 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "" ), 7 ), npos );

        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ) ), 0 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ), 1 ), 3 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ), 4 ), npos );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ), 6 ), npos );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "1" ), 7 ), npos );

        BOOST_TEST_EQ( sv.find( boost::core::string_view( "23" ) ), 1 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "23" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "23" ), 2 ), 4 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "23" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "23" ), 5 ), npos );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "23" ), 6 ), npos );
        BOOST_TEST_EQ( sv.find( boost::core::string_view( "23" ), 7 ), npos );

        BOOST_TEST_EQ( sv.find( '1' ), 0 );
        BOOST_TEST_EQ( sv.find( '1', 1 ), 3 );
        BOOST_TEST_EQ( sv.find( '1', 2 ), 3 );
        BOOST_TEST_EQ( sv.find( '1', 3 ), 3 );
        BOOST_TEST_EQ( sv.find( '1', 4 ), npos );
        BOOST_TEST_EQ( sv.find( '1', 5 ), npos );
        BOOST_TEST_EQ( sv.find( '1', 6 ), npos );
        BOOST_TEST_EQ( sv.find( '1', 7 ), npos );

        BOOST_TEST_EQ( sv.find( '3' ), 2 );
        BOOST_TEST_EQ( sv.find( '3', 1 ), 2 );
        BOOST_TEST_EQ( sv.find( '3', 2 ), 2 );
        BOOST_TEST_EQ( sv.find( '3', 3 ), 5 );
        BOOST_TEST_EQ( sv.find( '3', 4 ), 5 );
        BOOST_TEST_EQ( sv.find( '3', 5 ), 5 );
        BOOST_TEST_EQ( sv.find( '3', 6 ), npos );
        BOOST_TEST_EQ( sv.find( '3', 7 ), npos );

        BOOST_TEST_EQ( sv.find( '9' ), npos );

        BOOST_TEST_EQ( sv.find( "" ), 0 );
        BOOST_TEST_EQ( sv.find( "", 1 ), 1 );
        BOOST_TEST_EQ( sv.find( "", 6 ), 6 );
        BOOST_TEST_EQ( sv.find( "", 7 ), npos );

        BOOST_TEST_EQ( sv.find( "1" ), 0 );
        BOOST_TEST_EQ( sv.find( "1", 1 ), 3 );
        BOOST_TEST_EQ( sv.find( "1", 3 ), 3 );
        BOOST_TEST_EQ( sv.find( "1", 4 ), npos );
        BOOST_TEST_EQ( sv.find( "1", 6 ), npos );
        BOOST_TEST_EQ( sv.find( "1", 7 ), npos );

        BOOST_TEST_EQ( sv.find( "23" ), 1 );
        BOOST_TEST_EQ( sv.find( "23", 1 ), 1 );
        BOOST_TEST_EQ( sv.find( "23", 2 ), 4 );
        BOOST_TEST_EQ( sv.find( "23", 4 ), 4 );
        BOOST_TEST_EQ( sv.find( "23", 5 ), npos );
        BOOST_TEST_EQ( sv.find( "23", 6 ), npos );
        BOOST_TEST_EQ( sv.find( "23", 7 ), npos );

        BOOST_TEST_EQ( sv.find( "123", 0, 0 ), 0 );
        BOOST_TEST_EQ( sv.find( "123", 1, 0 ), 1 );
        BOOST_TEST_EQ( sv.find( "123", 6, 0 ), 6 );
        BOOST_TEST_EQ( sv.find( "123", 7, 0 ), npos );

        BOOST_TEST_EQ( sv.find( "123", 0, 1 ), 0 );
        BOOST_TEST_EQ( sv.find( "123", 1, 1 ), 3 );
        BOOST_TEST_EQ( sv.find( "123", 3, 1 ), 3 );
        BOOST_TEST_EQ( sv.find( "123", 4, 1 ), npos );
        BOOST_TEST_EQ( sv.find( "123", 6, 1 ), npos );
        BOOST_TEST_EQ( sv.find( "123", 7, 1 ), npos );

        BOOST_TEST_EQ( sv.find( "123", 0, 3 ), 0 );
        BOOST_TEST_EQ( sv.find( "123", 1, 3 ), 3 );
        BOOST_TEST_EQ( sv.find( "123", 3, 3 ), 3 );
        BOOST_TEST_EQ( sv.find( "123", 4, 3 ), npos );
        BOOST_TEST_EQ( sv.find( "123", 6, 3 ), npos );
        BOOST_TEST_EQ( sv.find( "123", 7, 3 ), npos );
    }

    return boost::report_errors();
}
