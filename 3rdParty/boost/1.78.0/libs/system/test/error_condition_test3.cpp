// Copyright 2020, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/error_condition.hpp>
#include <boost/system/generic_category.hpp>
#include <boost/system/system_category.hpp>
#include <boost/core/lightweight_test.hpp>

namespace sys = boost::system;

int main()
{
    char buffer[ 1024 ];

    sys::error_condition en;

    BOOST_TEST_EQ( en.value(), 0 );
    BOOST_TEST( en.category() == sys::generic_category() );
    BOOST_TEST_EQ( en.message(), en.category().message( en.value() ) );
    BOOST_TEST_CSTR_EQ( en.message( buffer, sizeof( buffer ) ), en.category().message( en.value(), buffer, sizeof( buffer ) ) );
    BOOST_TEST( !en.failed() );
    BOOST_TEST( !en );

    BOOST_TEST_EQ( en.to_string(), std::string( "cond:generic:0" ) );

    {
        sys::error_condition en2( en );

        BOOST_TEST_EQ( en2.value(), 0 );
        BOOST_TEST( en2.category() == sys::generic_category() );
        BOOST_TEST_EQ( en2.message(), en2.category().message( en2.value() ) );
        BOOST_TEST_CSTR_EQ( en2.message( buffer, sizeof( buffer ) ), en2.category().message( en2.value(), buffer, sizeof( buffer ) ) );
        BOOST_TEST( !en2.failed() );
        BOOST_TEST( !en2 );

        BOOST_TEST_EQ( en, en2 );
        BOOST_TEST_NOT( en != en2 );

        BOOST_TEST_EQ( en2.to_string(), std::string( "cond:generic:0" ) );
    }

    {
        sys::error_condition en2( en.value(), en.category() );

        BOOST_TEST_EQ( en2.value(), 0 );
        BOOST_TEST( en2.category() == sys::generic_category() );
        BOOST_TEST_EQ( en2.message(), en2.category().message( en2.value() ) );
        BOOST_TEST_CSTR_EQ( en2.message( buffer, sizeof( buffer ) ), en2.category().message( en2.value(), buffer, sizeof( buffer ) ) );
        BOOST_TEST( !en2.failed() );
        BOOST_TEST( !en2 );

        BOOST_TEST_EQ( en, en2 );
        BOOST_TEST_NOT( en != en2 );

        BOOST_TEST_EQ( en2.to_string(), std::string( "cond:generic:0" ) );
    }

    {
        sys::error_condition en2( 5, sys::generic_category() );

        BOOST_TEST_EQ( en2.value(), 5 );
        BOOST_TEST( en2.category() == sys::generic_category() );
        BOOST_TEST_EQ( en2.message(), en2.category().message( en2.value() ) );
        BOOST_TEST_CSTR_EQ( en2.message( buffer, sizeof( buffer ) ), en2.category().message( en2.value(), buffer, sizeof( buffer ) ) );
        BOOST_TEST( en2.failed() );
        BOOST_TEST( en2 );
        BOOST_TEST_NOT( !en2 );

        BOOST_TEST_NE( en, en2 );
        BOOST_TEST_NOT( en == en2 );

        BOOST_TEST_EQ( en2.to_string(), std::string( "cond:generic:5" ) );
    }

    {
        sys::error_condition en2( 5, sys::system_category() );

        BOOST_TEST_EQ( en2.value(), 5 );
        BOOST_TEST( en2.category() == sys::system_category() );
        BOOST_TEST_EQ( en2.message(), en2.category().message( en2.value() ) );
        BOOST_TEST_CSTR_EQ( en2.message( buffer, sizeof( buffer ) ), en2.category().message( en2.value(), buffer, sizeof( buffer ) ) );
        BOOST_TEST( en2.failed() );
        BOOST_TEST( en2 );
        BOOST_TEST_NOT( !en2 );

        BOOST_TEST_NE( en, en2 );
        BOOST_TEST_NOT( en == en2 );

        BOOST_TEST_EQ( en2.to_string(), std::string( "cond:system:5" ) );
    }

    return boost::report_errors();
}
