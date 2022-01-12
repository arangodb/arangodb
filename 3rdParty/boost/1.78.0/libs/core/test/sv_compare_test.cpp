// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <stdexcept>
#include <cstddef>

int main()
{
    std::size_t const npos = boost::core::string_view::npos;

    {
        boost::core::string_view sv1( "" );
        boost::core::string_view sv2( "" );

        BOOST_TEST_EQ( sv1.compare( sv2 ), 0 );

        BOOST_TEST_EQ( sv1.compare( 0, 0, sv2 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 1, sv2 ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 1, 0, sv2 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 1, 1, sv2 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 1, npos, sv2 ), std::out_of_range );

        BOOST_TEST_EQ( sv1.compare( 0, 0, sv2, 0, 0 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 1, sv2, 0, 1 ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 1, 0, sv2, 0, 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 1, 1, sv2, 0, 1 ), std::out_of_range );

        BOOST_TEST_THROWS( sv1.compare( 0, 0, sv2, 1, 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 0, 1, sv2, 1, 1 ), std::out_of_range );

        BOOST_TEST_EQ( sv1.compare( "" ), 0 );
        BOOST_TEST_LT( sv1.compare( "1" ), 0 );

        BOOST_TEST_EQ( sv1.compare( 0, 0, "" ), 0 );
        BOOST_TEST_LT( sv1.compare( 0, 0, "1" ), 0 );

        BOOST_TEST_EQ( sv1.compare( 0, 1, "" ), 0 );
        BOOST_TEST_LT( sv1.compare( 0, 1, "1" ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 1, 0, "" ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 1, 1, "1" ), std::out_of_range );

        BOOST_TEST_EQ( sv1.compare( 0, 0, "", 0 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 0, "1", 0 ), 0 );
        BOOST_TEST_LT( sv1.compare( 0, 0, "1", 1 ), 0 );

        BOOST_TEST_EQ( sv1.compare( 0, 1, "", 0 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 1, "1", 0 ), 0 );
        BOOST_TEST_LT( sv1.compare( 0, 1, "1", 1 ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 1, 0, "", 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 1, 1, "1", 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 1, 1, "1", 1 ), std::out_of_range );
    }

    {
        boost::core::string_view sv1( "123412345" );
        boost::core::string_view sv2( "1234" );

        BOOST_TEST_GT( sv1.compare( sv2 ), 0 );

        BOOST_TEST_LT( sv1.compare( 0, 3, sv2 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 4, sv2 ), 0 );
        BOOST_TEST_GT( sv1.compare( 0, 5, sv2 ), 0 );
        BOOST_TEST_GT( sv1.compare( 0, npos, sv2 ), 0 );

        BOOST_TEST_LT( sv1.compare( 1, 0, sv2 ), 0 );
        BOOST_TEST_GT( sv1.compare( 1, 1, sv2 ), 0 );

        BOOST_TEST_LT( sv1.compare( 4, 3, sv2 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 4, 4, sv2 ), 0 );
        BOOST_TEST_GT( sv1.compare( 4, 5, sv2 ), 0 );

        BOOST_TEST_LT( sv1.compare( 9, 0, sv2 ), 0 );
        BOOST_TEST_LT( sv1.compare( 9, 1, sv2 ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 10, 0, sv2 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 10, 1, sv2 ), std::out_of_range );

        BOOST_TEST_GT( sv1.compare( 0, 3, sv2, 0, 2 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 3, sv2, 0, 3 ), 0 );
        BOOST_TEST_LT( sv1.compare( 0, 3, sv2, 0, 4 ), 0 );
        BOOST_TEST_LT( sv1.compare( 0, 3, sv2, 0, 5 ), 0 );

        BOOST_TEST_GT( sv1.compare( 0, 4, sv2, 0, 3 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 4, sv2, 0, 4 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 0, 4, sv2, 0, 5 ), 0 );

        BOOST_TEST_LT( sv1.compare( 5, 2, sv2, 1, npos ), 0 );
        BOOST_TEST_EQ( sv1.compare( 5, 3, sv2, 1, npos ), 0 );
        BOOST_TEST_GT( sv1.compare( 5, 4, sv2, 1, npos ), 0 );

        BOOST_TEST_EQ( sv1.compare( 9, 0, sv2, 0, 0 ), 0 );
        BOOST_TEST_LT( sv1.compare( 9, 1, sv2, 0, 1 ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 10, 0, sv2, 0, 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 10, 1, sv2, 0, 1 ), std::out_of_range );

        BOOST_TEST_EQ( sv1.compare( 0, 0, sv2, 4, 0 ), 0 );
        BOOST_TEST_GT( sv1.compare( 0, 1, sv2, 4, 1 ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 0, 0, sv2, 5, 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 0, 1, sv2, 5, 1 ), std::out_of_range );

        BOOST_TEST_GT( sv1.compare( "" ), 0 );
        BOOST_TEST_GT( sv1.compare( "12341234" ), 0 );
        BOOST_TEST_GT( sv1.compare( "123412344" ), 0 );
        BOOST_TEST_EQ( sv1.compare( "123412345" ), 0 );
        BOOST_TEST_LT( sv1.compare( "123412346" ), 0 );
        BOOST_TEST_LT( sv1.compare( "1234123456" ), 0 );

        BOOST_TEST_GT( sv1.compare( 4, 3, "" ), 0 );
        BOOST_TEST_GT( sv1.compare( 4, 3, "1" ), 0 );
        BOOST_TEST_GT( sv1.compare( 4, 3, "12" ), 0 );
        BOOST_TEST_GT( sv1.compare( 4, 3, "122" ), 0 );
        BOOST_TEST_EQ( sv1.compare( 4, 3, "123" ), 0 );
        BOOST_TEST_LT( sv1.compare( 4, 3, "124" ), 0 );
        BOOST_TEST_LT( sv1.compare( 4, 3, "1234" ), 0 );

        BOOST_TEST_EQ( sv1.compare( 9, 0, "" ), 0 );
        BOOST_TEST_LT( sv1.compare( 9, 1, "1" ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 10, 0, "" ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 10, 1, "1" ), std::out_of_range );

        BOOST_TEST_GT( sv1.compare( 4, npos, "123456", 4 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 4, npos, "123456", 5 ), 0 );
        BOOST_TEST_LT( sv1.compare( 4, npos, "123456", 6 ), 0 );

        BOOST_TEST_EQ( sv1.compare( 9, npos, "", 0 ), 0 );
        BOOST_TEST_EQ( sv1.compare( 9, npos, "1", 0 ), 0 );
        BOOST_TEST_LT( sv1.compare( 9, npos, "1", 1 ), 0 );

        BOOST_TEST_THROWS( sv1.compare( 10, npos, "", 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 10, npos, "1", 0 ), std::out_of_range );
        BOOST_TEST_THROWS( sv1.compare( 10, npos, "1", 1 ), std::out_of_range );
    }

    return boost::report_errors();
}
