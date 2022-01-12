// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>
#include <algorithm>

int main()
{
    {
        boost::core::string_view sv;

        {
            std::string s( sv.begin(), sv.end() );
            BOOST_TEST_EQ( s, std::string( "" ) );
        }

        {
            std::string s( sv.cbegin(), sv.cend() );
            BOOST_TEST_EQ( s, std::string( "" ) );
        }

        {
            std::string s( sv.rbegin(), sv.rend() );
            BOOST_TEST_EQ( s, std::string( "" ) );
        }

        {
            std::string s( sv.crbegin(), sv.crend() );
            BOOST_TEST_EQ( s, std::string( "" ) );
        }
    }

    {
        boost::core::string_view sv( "123" );

        {
            std::string s( sv.begin(), sv.end() );
            BOOST_TEST_EQ( s, std::string( "123" ) );
        }

        {
            std::string s( sv.cbegin(), sv.cend() );
            BOOST_TEST_EQ( s, std::string( "123" ) );
        }

        {
            std::string s( sv.rbegin(), sv.rend() );
            BOOST_TEST_EQ( s, std::string( "321" ) );
        }

        {
            std::string s( sv.crbegin(), sv.crend() );
            BOOST_TEST_EQ( s, std::string( "321" ) );
        }
    }

    return boost::report_errors();
}
