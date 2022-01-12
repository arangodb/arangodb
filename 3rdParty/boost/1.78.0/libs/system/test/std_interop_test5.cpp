// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config/pragma_message.hpp>
#include <cerrno>

#if !defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

BOOST_PRAGMA_MESSAGE( "BOOST_SYSTEM_HAS_SYSTEM_ERROR not defined, test will be skipped" )
int main() {}

#else

#include <system_error>

int main()
{
    {
        std::error_code e1;
        std::error_condition en = e1.default_error_condition();

        BOOST_TEST( e1 == en );
        BOOST_TEST_NOT( e1 != en );

        boost::system::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

        BOOST_TEST( e2 == en );
        BOOST_TEST_NOT( e2 != en );

        std::error_code e3( e2 );

        BOOST_TEST( e3 == en );
        BOOST_TEST_NOT( e3 != en );
    }

    {
        std::error_code e1( 5, std::system_category() );
        std::error_condition en = e1.default_error_condition();

        BOOST_TEST( e1 == en );
        BOOST_TEST_NOT( e1 != en );

        boost::system::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

        BOOST_TEST( e2 == en );
        BOOST_TEST_NOT( e2 != en );

        std::error_code e3( e2 );

        BOOST_TEST( e3 == en );
        BOOST_TEST_NOT( e3 != en );
    }

    {
        std::error_code e1( 0, std::generic_category() );
        std::error_condition en = e1.default_error_condition();

        BOOST_TEST( e1 == en );
        BOOST_TEST_NOT( e1 != en );

        boost::system::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

        BOOST_TEST( e2 == en );
        BOOST_TEST_NOT( e2 != en );

        std::error_code e3( e2 );

        BOOST_TEST( e3 == en );
        BOOST_TEST_NOT( e3 != en );
    }

    {
        std::error_code e1( 5, std::generic_category() );
        std::error_condition en = e1.default_error_condition();

        BOOST_TEST( e1 == en );
        BOOST_TEST_NOT( e1 != en );

        boost::system::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

        BOOST_TEST( e2 == en );
        BOOST_TEST_NOT( e2 != en );

        std::error_code e3( e2 );

        BOOST_TEST( e3 == en );
        BOOST_TEST_NOT( e3 != en );
    }

    return boost::report_errors();
}

#endif
