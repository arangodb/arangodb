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

void f( std::error_code& e1, std::error_code e2 )
{
    e1 = e2;
}

int main()
{
    {
        boost::system::error_code e1;
        std::error_code e2;

        f( e1, e2 );

        BOOST_TEST_EQ( e1, boost::system::error_code( e2 ) );

        std::error_code e3( e1 );
        BOOST_TEST_EQ( e3, e2 );
    }

    {
        boost::system::error_code e1;
        std::error_code e2( 0, std::system_category() );

        f( e1, e2 );

        BOOST_TEST_EQ( e1, boost::system::error_code( e2 ) );

        std::error_code e3( e1 );
        BOOST_TEST_EQ( e3, e2 );
    }

    {
        boost::system::error_code e1;
        std::error_code e2( 5, std::system_category() );

        f( e1, e2 );

        BOOST_TEST_EQ( e1, boost::system::error_code( e2 ) );

        std::error_code e3( e1 );
        BOOST_TEST_EQ( e3, e2 );
    }

    {
        boost::system::error_code e1;
        std::error_code e2( 0, std::generic_category() );

        f( e1, e2 );

        BOOST_TEST_EQ( e1, boost::system::error_code( e2 ) );

        std::error_code e3( e1 );
        BOOST_TEST_EQ( e3, e2 );
    }

    {
        boost::system::error_code e1;
        std::error_code e2( ENOENT, std::generic_category() );

        f( e1, e2 );

        BOOST_TEST_EQ( e1, boost::system::error_code( e2 ) );

        std::error_code e3( e1 );
        BOOST_TEST_EQ( e3, e2 );
    }

    return boost::report_errors();
}

#endif
