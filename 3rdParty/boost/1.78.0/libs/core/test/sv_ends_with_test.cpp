// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <stdexcept>
#include <cstddef>

int main()
{
    {
        boost::core::string_view sv( "" );

        BOOST_TEST( sv.ends_with( boost::core::string_view() ) );
        BOOST_TEST( sv.ends_with( boost::core::string_view( "" ) ) );
        BOOST_TEST( sv.ends_with( "" ) );

        BOOST_TEST( !sv.ends_with( boost::core::string_view( "1" ) ) );
        BOOST_TEST( !sv.ends_with( '1' ) );
        BOOST_TEST( !sv.ends_with( "1" ) );
    }

    {
        boost::core::string_view sv( "123" );

        BOOST_TEST( sv.ends_with( boost::core::string_view() ) );
        BOOST_TEST( sv.ends_with( boost::core::string_view( "" ) ) );
        BOOST_TEST( sv.ends_with( "" ) );

        BOOST_TEST( sv.ends_with( boost::core::string_view( "3" ) ) );
        BOOST_TEST( sv.ends_with( '3' ) );
        BOOST_TEST( sv.ends_with( "3" ) );

        BOOST_TEST( sv.ends_with( boost::core::string_view( "23" ) ) );
        BOOST_TEST( sv.ends_with( "23" ) );

        BOOST_TEST( sv.ends_with( boost::core::string_view( "123" ) ) );
        BOOST_TEST( sv.ends_with( "123" ) );

        BOOST_TEST( !sv.ends_with( boost::core::string_view( "1234" ) ) );
        BOOST_TEST( !sv.ends_with( "1234" ) );

        BOOST_TEST( !sv.ends_with( boost::core::string_view( "2" ) ) );
        BOOST_TEST( !sv.ends_with( '2' ) );
        BOOST_TEST( !sv.ends_with( "2" ) );
    }

    return boost::report_errors();
}
