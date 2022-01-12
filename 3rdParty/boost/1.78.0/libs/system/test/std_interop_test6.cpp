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
        boost::system::error_code e1;
        boost::system::error_condition en = e1.default_error_condition();

        BOOST_TEST_EQ( e1, en );
        BOOST_TEST_NOT( e1 != en );

        std::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

#if defined(_LIBCPP_VERSION)

        // Under MS STL and libstdc++, std::error_code() does not compare
        // equal to std::error_condition(). Go figure.

        BOOST_TEST_EQ( e2, en );
        BOOST_TEST_NOT( e2 != en );

        boost::system::error_code e3( e2 );

        BOOST_TEST_EQ( e3, en );
        BOOST_TEST_NOT( e3 != en );

#endif
    }

    {
        boost::system::error_code e1( 0, boost::system::system_category() );
        boost::system::error_condition en = e1.default_error_condition();

        BOOST_TEST_EQ( e1, en );
        BOOST_TEST_NOT( e1 != en );

        std::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

#if defined(_LIBCPP_VERSION)

        BOOST_TEST_EQ( e2, en );
        BOOST_TEST_NOT( e2 != en );

        boost::system::error_code e3( e2 );

        BOOST_TEST_EQ( e3, en );
        BOOST_TEST_NOT( e3 != en );

#endif
    }

    {
        boost::system::error_code e1( 5, boost::system::system_category() );
        boost::system::error_condition en = e1.default_error_condition();

        BOOST_TEST_EQ( e1, en );
        BOOST_TEST_NOT( e1 != en );

        std::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

        BOOST_TEST_EQ( e2, en );
        BOOST_TEST_NOT( e2 != en );

        boost::system::error_code e3( e2 );

        BOOST_TEST_EQ( e3, en );
        BOOST_TEST_NOT( e3 != en );
    }

    {
        boost::system::error_code e1( 0, boost::system::generic_category() );
        boost::system::error_condition en = e1.default_error_condition();

        BOOST_TEST_EQ( e1, en );
        BOOST_TEST_NOT( e1 != en );

        std::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

        BOOST_TEST_EQ( e2, en );
        BOOST_TEST_NOT( e2 != en );

        boost::system::error_code e3( e2 );

        BOOST_TEST_EQ( e3, en );
        BOOST_TEST_NOT( e3 != en );
    }

    {
        boost::system::error_code e1( 5, boost::system::generic_category() );
        boost::system::error_condition en = e1.default_error_condition();

        BOOST_TEST_EQ( e1, en );
        BOOST_TEST_NOT( e1 != en );

        std::error_code e2( e1 );

        BOOST_TEST_EQ( e2, e1 );
        BOOST_TEST_NOT( e2 != e1 );

        BOOST_TEST_EQ( e2, en );
        BOOST_TEST_NOT( e2 != en );

        boost::system::error_code e3( e2 );

        BOOST_TEST_EQ( e3, en );
        BOOST_TEST_NOT( e3 != en );
    }

    return boost::report_errors();
}

#endif
