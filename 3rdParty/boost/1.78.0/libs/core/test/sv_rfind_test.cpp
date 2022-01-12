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

        BOOST_TEST_EQ( sv.rfind( boost::core::string_view() ), 0 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view(), 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view(), 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ) ), 0 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ), 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ), 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ) ), npos );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 1 ), npos );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 0 ), npos );

        BOOST_TEST_EQ( sv.rfind( '1' ), npos );
        BOOST_TEST_EQ( sv.rfind( '1', 1 ), npos );
        BOOST_TEST_EQ( sv.rfind( '1', 0 ), npos );

        BOOST_TEST_EQ( sv.rfind( "" ), 0 );
        BOOST_TEST_EQ( sv.rfind( "", 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "", 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( "1" ), npos );
        BOOST_TEST_EQ( sv.rfind( "1", 1 ), npos );
        BOOST_TEST_EQ( sv.rfind( "1", 0 ), npos );

        BOOST_TEST_EQ( sv.rfind( "1", npos, 0 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "1", 1, 0 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "1", 0, 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( "1", npos, 1 ), npos );
        BOOST_TEST_EQ( sv.rfind( "1", 1, 1 ), npos );
        BOOST_TEST_EQ( sv.rfind( "1", 0, 1 ), npos );
    }

    {
        boost::core::string_view sv( "123123" );

        BOOST_TEST_EQ( sv.rfind( boost::core::string_view() ), 6 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view(), 7 ), 6 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view(), 6 ), 6 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view(), 5 ), 5 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view(), 1 ), 1 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view(), 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ) ), 6 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ), 7 ), 6 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ), 6 ), 6 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ), 5 ), 5 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "" ), 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ) ), 3 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 7 ), 3 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 6 ), 3 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 5 ), 3 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 4 ), 3 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 2 ), 0 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "1" ), 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ) ), 4 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 7 ), 4 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 6 ), 4 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 5 ), 4 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 3 ), 1 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 2 ), 1 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.rfind( boost::core::string_view( "23" ), 0 ), npos );

        BOOST_TEST_EQ( sv.rfind( '1' ), 3 );
        BOOST_TEST_EQ( sv.rfind( '1', 7 ), 3 );
        BOOST_TEST_EQ( sv.rfind( '1', 6 ), 3 );
        BOOST_TEST_EQ( sv.rfind( '1', 5 ), 3 );
        BOOST_TEST_EQ( sv.rfind( '1', 4 ), 3 );
        BOOST_TEST_EQ( sv.rfind( '1', 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( '1', 2 ), 0 );
        BOOST_TEST_EQ( sv.rfind( '1', 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( '1', 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( '3' ), 5 );
        BOOST_TEST_EQ( sv.rfind( '3', 7 ), 5 );
        BOOST_TEST_EQ( sv.rfind( '3', 6 ), 5 );
        BOOST_TEST_EQ( sv.rfind( '3', 5 ), 5 );
        BOOST_TEST_EQ( sv.rfind( '3', 4 ), 2 );
        BOOST_TEST_EQ( sv.rfind( '3', 3 ), 2 );
        BOOST_TEST_EQ( sv.rfind( '3', 2 ), 2 );
        BOOST_TEST_EQ( sv.rfind( '3', 1 ), npos );
        BOOST_TEST_EQ( sv.rfind( '3', 0 ), npos );

        BOOST_TEST_EQ( sv.rfind( '9' ), npos );
        BOOST_TEST_EQ( sv.rfind( '9', 7 ), npos );
        BOOST_TEST_EQ( sv.rfind( '9', 6 ), npos );

        BOOST_TEST_EQ( sv.rfind( "" ), 6 );
        BOOST_TEST_EQ( sv.rfind( "", 7 ), 6 );
        BOOST_TEST_EQ( sv.rfind( "", 6 ), 6 );
        BOOST_TEST_EQ( sv.rfind( "", 5 ), 5 );
        BOOST_TEST_EQ( sv.rfind( "", 1 ), 1 );
        BOOST_TEST_EQ( sv.rfind( "", 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( "1" ), 3 );
        BOOST_TEST_EQ( sv.rfind( "1", 7 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "1", 6 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "1", 5 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "1", 4 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "1", 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "1", 2 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "1", 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "1", 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( "23" ), 4 );
        BOOST_TEST_EQ( sv.rfind( "23", 7 ), 4 );
        BOOST_TEST_EQ( sv.rfind( "23", 6 ), 4 );
        BOOST_TEST_EQ( sv.rfind( "23", 5 ), 4 );
        BOOST_TEST_EQ( sv.rfind( "23", 4 ), 4 );
        BOOST_TEST_EQ( sv.rfind( "23", 3 ), 1 );
        BOOST_TEST_EQ( sv.rfind( "23", 2 ), 1 );
        BOOST_TEST_EQ( sv.rfind( "23", 1 ), 1 );
        BOOST_TEST_EQ( sv.rfind( "23", 0 ), npos );

        BOOST_TEST_EQ( sv.rfind( "123", npos, 0 ), 6 );
        BOOST_TEST_EQ( sv.rfind( "123", 7, 0 ), 6 );
        BOOST_TEST_EQ( sv.rfind( "123", 6, 0 ), 6 );
        BOOST_TEST_EQ( sv.rfind( "123", 5, 0 ), 5 );
        BOOST_TEST_EQ( sv.rfind( "123", 1, 0 ), 1 );
        BOOST_TEST_EQ( sv.rfind( "123", 0, 0 ), 0 );

        BOOST_TEST_EQ( sv.rfind( "123", npos, 1 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 7, 1 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 6, 1 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 5, 1 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 4, 1 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 3, 1 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 2, 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "123", 1, 1 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "123", 0, 1 ), 0 );

        BOOST_TEST_EQ( sv.rfind( "123", npos, 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 7, 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 6, 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 5, 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 4, 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 3, 3 ), 3 );
        BOOST_TEST_EQ( sv.rfind( "123", 2, 3 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "123", 1, 3 ), 0 );
        BOOST_TEST_EQ( sv.rfind( "123", 0, 3 ), 0 );

        BOOST_TEST_EQ( sv.rfind( "123123" ), 0 );
        BOOST_TEST_EQ( sv.rfind( "1231231" ), npos );
    }

    return boost::report_errors();
}
