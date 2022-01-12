// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <algorithm>
#include <cstddef>

int main()
{
    std::size_t const npos = boost::core::string_view::npos;

    {
        boost::core::string_view sv( "" );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view() ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ) ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ) ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( '1' ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( "" ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( "1" ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( "12", 0, 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "12", 1, 0 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( "12", 0, 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "12", 1, 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( "12", 0, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "12", 1, 2 ), npos );
    }

    {
        boost::core::wstring_view sv( L"" );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view() ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ) ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ) ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( L'1' ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( L"" ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( L"1" ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( L"12", 0, 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"12", 1, 0 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( L"12", 0, 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"12", 1, 1 ), npos );

        BOOST_TEST_EQ( sv.find_last_not_of( L"12", 0, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"12", 1, 2 ), npos );
    }

    {
        boost::core::string_view sv( "123123" );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view(), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view() ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "" ) ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 3 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "1" ) ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "4" ) ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 1 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 2 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ), 7 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::string_view( "23" ) ), 3 );

        BOOST_TEST_EQ( sv.find_last_not_of( '1', 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 3 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( '1', 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( '1' ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( '3', 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3', 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3', 2 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3', 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3', 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3', 5 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3', 6 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3', 7 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( '3' ), 4 );

        BOOST_TEST_EQ( sv.find_last_not_of( '9', 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9', 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9', 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9', 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9', 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9', 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9', 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9', 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( '9' ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( "", 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "", 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "" ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( "1", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 3 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "1", 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "1" ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( "23", 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23", 1 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23", 2 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23", 7 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "23" ), 3 );

        BOOST_TEST_EQ( sv.find_last_not_of( "123", 0, 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 1, 0 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 2, 0 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 3, 0 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 4, 0 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 5, 0 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 6, 0 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 7, 0 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( "123", 0, 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 1, 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 2, 1 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 3, 1 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 4, 1 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 5, 1 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 6, 1 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 7, 1 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( "123", 0, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 1, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 2, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 3, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 4, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 5, 2 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 6, 2 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 7, 2 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( "123", 0, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 1, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 2, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 3, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 4, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 5, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 6, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "123", 7, 3 ), npos );
    }

    {
        boost::core::wstring_view sv( L"123123" );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view(), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view() ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"" ) ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 3 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"1" ) ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ), 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"4" ) ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 1 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 2 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ), 7 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( boost::core::wstring_view( L"23" ) ), 3 );

        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 3 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1', 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'1' ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 2 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 5 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 6 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3', 7 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'3' ), 4 );

        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9', 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L'9' ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"", 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"", 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"" ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 3 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1", 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"1" ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 1 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 2 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23", 7 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"23" ), 3 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 0, 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 1, 0 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 2, 0 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 3, 0 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 4, 0 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 5, 0 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 6, 0 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 7, 0 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 0, 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 1, 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 2, 1 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 3, 1 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 4, 1 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 5, 1 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 6, 1 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 7, 1 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 0, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 1, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 2, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 3, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 4, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 5, 2 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 6, 2 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 7, 2 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 0, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 1, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 2, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 3, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 4, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 5, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 6, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"123", 7, 3 ), npos );
    }

    {
        boost::core::wstring_view sv( L"\x101\x102\x103\x101\x102\x103" );

        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 0, 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 1, 0 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 2, 0 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 3, 0 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 4, 0 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 5, 0 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 6, 0 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 7, 0 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 0, 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 1, 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 2, 1 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 3, 1 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 4, 1 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 5, 1 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 6, 1 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 7, 1 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 0, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 1, 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 2, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 3, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 4, 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 5, 2 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 6, 2 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 7, 2 ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 0, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 1, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 2, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 3, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 4, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 5, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 6, 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"\x101\x102\x103", 7, 3 ), npos );
    }

    {
        boost::core::string_view sv( "123a123B123c" );

        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 7 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 8 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 9 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 10 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 11 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789", 12 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( "0123456789" ), 11 );
    }

    {
        boost::core::wstring_view sv( L"123a123B123c" );

        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 7 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 8 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 9 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 10 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 11 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789", 12 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"0123456789" ), 11 );
    }

    {
        boost::core::string_view sv( "abc1abc2abc3" );

        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 7 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 8 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 9 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 10 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 11 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 12 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" ), 11 );
    }

    {
        boost::core::wstring_view sv( L"abc1abc2abc3" );

        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 7 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 8 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 9 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 10 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 11 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 12 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" ), 11 );
    }

    {
        char str[ 256 ];

        for( int i = 0; i < 256; ++i )
        {
            str[ i ] = static_cast< unsigned char >( i );
        }

        boost::core::string_view sv( str, 256 );

        BOOST_TEST_EQ( sv.find_last_not_of( sv ), npos );

        std::string str2( sv.data(), sv.size() );

        for( int i = 0; i < 256; ++i )
        {
            std::string str3( str2 );

            str3[ i ] = ~str3[ i ];

            BOOST_TEST_EQ( sv.find_last_not_of( str3 ), i );
        }

        std::reverse( str, str + 256 );

        for( int i = 0; i < 256; ++i )
        {
            std::string str3( str2 );

            str3[ i ] = ~str3[ i ];

            BOOST_TEST_EQ( sv.find_last_not_of( str3 ), 255 - i );
        }
    }

    {
        wchar_t str[ 256 ];

        for( int i = 0; i < 256; ++i )
        {
            str[ i ] = static_cast< wchar_t >( 0x100 + i );
        }

        boost::core::wstring_view sv( str, 256 );

        BOOST_TEST_EQ( sv.find_first_not_of( sv ), npos );

        std::wstring str2( sv.data(), sv.size() );

        for( int i = 0; i < 256; ++i )
        {
            std::wstring str3( str2 );

            str3[ i ] = ~str3[ i ];

            BOOST_TEST_EQ( sv.find_first_not_of( str3 ), i );
        }

        std::reverse( str, str + 256 );

        for( int i = 0; i < 256; ++i )
        {
            std::wstring str3( str2 );

            str3[ i ] = ~str3[ i ];

            BOOST_TEST_EQ( sv.find_first_not_of( str3 ), 255 - i );
        }
    }

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

    {
        boost::core::u8string_view sv( u8"123123" );

        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"", 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"" ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 1 ), 1 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 2 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 3 ), 2 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 4 ), 4 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 5 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 6 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1", 7 ), 5 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"1" ), 5 );

        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 0 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 1 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 2 ), 0 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23", 7 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"23" ), 3 );

        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 3 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 4 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 5 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 6 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"123", 7 ), npos );
    }

    {
        boost::core::u8string_view sv( u8"123a123B123c" );

        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 7 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 8 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 9 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 10 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 11 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789", 12 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"0123456789" ), 11 );
    }

    {
        boost::core::u8string_view sv( u8"abc1abc2abc3" );

        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 0 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 1 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 2 ), npos );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 3 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 4 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 5 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 6 ), 3 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 7 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 8 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 9 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 10 ), 7 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 11 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 12 ), 11 );
        BOOST_TEST_EQ( sv.find_last_not_of( u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" ), 11 );
    }

#endif

    return boost::report_errors();
}
