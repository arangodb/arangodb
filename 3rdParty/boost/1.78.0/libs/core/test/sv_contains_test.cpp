// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

int main()
{
    {
        boost::core::string_view sv( "" );

        BOOST_TEST( sv.contains( boost::core::string_view() ) );

        BOOST_TEST( sv.contains( boost::core::string_view( "" ) ) );
        BOOST_TEST( !sv.contains( boost::core::string_view( "1" ) ) );

        BOOST_TEST( !sv.contains( '1' ) );

        BOOST_TEST( sv.contains( "" ) );
        BOOST_TEST( !sv.contains( "1" ) );
    }

    {
        boost::core::string_view sv( "123123" );

        BOOST_TEST( sv.contains( boost::core::string_view() ) );
        BOOST_TEST( sv.contains( boost::core::string_view( "" ) ) );

        BOOST_TEST( sv.contains( boost::core::string_view( "1" ) ) );
        BOOST_TEST( sv.contains( boost::core::string_view( "2" ) ) );
        BOOST_TEST( sv.contains( boost::core::string_view( "3" ) ) );
        BOOST_TEST( !sv.contains( boost::core::string_view( "4" ) ) );

        BOOST_TEST( sv.contains( boost::core::string_view( "12" ) ) );
        BOOST_TEST( sv.contains( boost::core::string_view( "23" ) ) );
        BOOST_TEST( !sv.contains( boost::core::string_view( "34" ) ) );
        BOOST_TEST( !sv.contains( boost::core::string_view( "21" ) ) );

        BOOST_TEST( sv.contains( '1' ) );
        BOOST_TEST( sv.contains( '2' ) );
        BOOST_TEST( sv.contains( '3' ) );
        BOOST_TEST( !sv.contains( '4' ) );

        BOOST_TEST( sv.contains( "" ) );

        BOOST_TEST( sv.contains( "1" ) );
        BOOST_TEST( sv.contains( "2" ) );
        BOOST_TEST( sv.contains( "3" ) );
        BOOST_TEST( !sv.contains( "4" ) );

        BOOST_TEST( sv.contains( "12" ) );
        BOOST_TEST( sv.contains( "23" ) );
        BOOST_TEST( !sv.contains( "34" ) );
        BOOST_TEST( !sv.contains( "21" ) );

        BOOST_TEST( sv.contains( "123" ) );
        BOOST_TEST( !sv.contains( "234" ) );
        BOOST_TEST( sv.contains( "231" ) );
        BOOST_TEST( !sv.contains( "321" ) );

        BOOST_TEST( !sv.contains( "1234" ) );
        BOOST_TEST( sv.contains( "1231" ) );

        BOOST_TEST( sv.contains( "123123" ) );
        BOOST_TEST( !sv.contains( "1231231" ) );
    }

    return boost::report_errors();
}
