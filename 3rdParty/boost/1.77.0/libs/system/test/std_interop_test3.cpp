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
        boost::system::error_code e2 = e1;

        BOOST_TEST( !e2.failed() );
        BOOST_TEST_EQ( e1.message(), e2.message() );

        std::error_code e3 = e2;
        BOOST_TEST_EQ( e1, e3 );
    }

    {
        std::error_code e1( 5, std::system_category() );
        boost::system::error_code e2 = e1;

        BOOST_TEST( e2.failed() );
        BOOST_TEST_EQ( e1.message(), e2.message() );

        std::error_code e3 = e2;
        BOOST_TEST_EQ( e1, e3 );
    }

    {
        std::error_code e1( 0, std::generic_category() );
        boost::system::error_code e2 = e1;

        BOOST_TEST( !e2.failed() );
        BOOST_TEST_EQ( e1.message(), e2.message() );

        std::error_code e3 = e2;
        BOOST_TEST_EQ( e1, e3 );
    }

    {
        std::error_code e1( ENOENT, std::generic_category() );
        boost::system::error_code e2 = e1;

        BOOST_TEST( e2.failed() );
        BOOST_TEST_EQ( e1.message(), e2.message() );

        std::error_code e3 = e2;
        BOOST_TEST_EQ( e1, e3 );
    }

    {
        std::error_code e1 = make_error_code( std::errc::no_such_file_or_directory );
        boost::system::error_code e2 = e1;

        BOOST_TEST( e2.failed() );
        BOOST_TEST_EQ( e1.message(), e2.message() );

        std::error_code e3 = e2;
        BOOST_TEST_EQ( e1, e3 );
    }

    return boost::report_errors();
}

#endif
