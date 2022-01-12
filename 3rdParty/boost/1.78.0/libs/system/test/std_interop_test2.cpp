// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/system/error_code.hpp>
#include <boost/system/system_category.hpp>
#include <boost/system/generic_category.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config/pragma_message.hpp>
#include <cerrno>

#if !defined(BOOST_SYSTEM_HAS_SYSTEM_ERROR)

BOOST_PRAGMA_MESSAGE( "BOOST_SYSTEM_HAS_SYSTEM_ERROR not defined, test will be skipped" )
int main() {}

#else

#include <system_error>

void f1( std::error_code ec, int value, std::error_category const& category )
{
    BOOST_TEST_EQ( ec.value(), value );
    BOOST_TEST_EQ( &ec.category(), &category );
}

void f2( std::error_code const& ec, int value, std::error_category const& category )
{
    BOOST_TEST_EQ( ec.value(), value );
    BOOST_TEST_EQ( &ec.category(), &category );
}

int main()
{
    {
        boost::system::error_code e1;
        boost::system::error_code e2( e1 );

        f1( e1, e1.value(), e1.category() );
#if !defined(BOOST_SYSTEM_CLANG_6)
        f2( e1, e1.value(), e1.category() );
#endif

        BOOST_TEST_EQ( e1, e2 );
    }

    {
        boost::system::error_code e1( 0, boost::system::system_category() );
        boost::system::error_code e2( e1 );

        f1( e1, e1.value(), e1.category() );
#if !defined(BOOST_SYSTEM_CLANG_6)
        f2( e1, e1.value(), e1.category() );
#endif

        BOOST_TEST_EQ( e1, e2 );
    }

    {
        boost::system::error_code e1( 5, boost::system::system_category() );
        boost::system::error_code e2( e1 );

        f1( e1, e1.value(), e1.category() );
#if !defined(BOOST_SYSTEM_CLANG_6)
        f2( e1, e1.value(), e1.category() );
#endif

        BOOST_TEST_EQ( e1, e2 );
    }

    {
        boost::system::error_code e1( 0, boost::system::generic_category() );
        boost::system::error_code e2( e1 );

        f1( e1, e1.value(), e1.category() );
#if !defined(BOOST_SYSTEM_CLANG_6)
        f2( e1, e1.value(), e1.category() );
#endif

        BOOST_TEST_EQ( e1, e2 );
    }

    {
        boost::system::error_code e1( ENOENT, boost::system::generic_category() );
        boost::system::error_code e2( e1 );

        f1( e1, e1.value(), e1.category() );
#if !defined(BOOST_SYSTEM_CLANG_6)
        f2( e1, e1.value(), e1.category() );
#endif

        BOOST_TEST_EQ( e1, e2 );
    }

    return boost::report_errors();
}

#endif
