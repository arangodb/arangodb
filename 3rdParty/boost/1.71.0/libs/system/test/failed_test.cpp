
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

// Avoid spurious VC++ warnings
#define _CRT_SECURE_NO_WARNINGS

#include <boost/system/error_code.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstdio>

using namespace boost::system;

struct http_category_impl: public error_category
{
    // clang++ 3.8 and below: initialization of const object
    // requires a user-provided default constructor
    BOOST_SYSTEM_CONSTEXPR http_category_impl() BOOST_NOEXCEPT
    {
    }

    char const * name() const BOOST_NOEXCEPT
    {
        return "http";
    }

    std::string message( int ev ) const
    {
        char buffer[ 32 ];

        std::sprintf( buffer, "HTTP/1.0 %d", ev );
        return buffer;
    }

    bool failed( int ev ) const BOOST_NOEXCEPT
    {
        return !( ev >= 200 && ev < 300 );
    }
};

error_category const & http_category()
{
    static const http_category_impl instance;
    return instance;
}

#define TEST_NOT_FAILED(ec) BOOST_TEST( !ec.failed() ); BOOST_TEST( ec? false: true ); BOOST_TEST( !ec );
#define TEST_FAILED(ec) BOOST_TEST( ec.failed() ); BOOST_TEST( ec ); BOOST_TEST( !!ec );

template<class Ec> void test()
{
    {
        Ec ec;
        TEST_NOT_FAILED( ec );

        ec.assign( 1, generic_category() );
        TEST_FAILED( ec );

        ec.clear();
        TEST_NOT_FAILED( ec );

        ec = Ec( 1, generic_category() );
        TEST_FAILED( ec );

        ec = Ec();
        TEST_NOT_FAILED( ec );
    }

    {
        Ec ec;
        TEST_NOT_FAILED( ec );

        ec.assign( 1, system_category() );
        TEST_FAILED( ec );

        ec.clear();
        TEST_NOT_FAILED( ec );

        ec = Ec( 1, system_category() );
        TEST_FAILED( ec );

        ec = Ec();
        TEST_NOT_FAILED( ec );
    }

    {
        Ec ec( 0, generic_category() );
        TEST_NOT_FAILED( ec );

        ec.assign( 1, system_category() );
        TEST_FAILED( ec );

        ec = Ec( 0, system_category() );
        TEST_NOT_FAILED( ec );
    }

    {
        Ec ec( 1, generic_category() );
        TEST_FAILED( ec );

        ec.assign( 0, system_category() );
        TEST_NOT_FAILED( ec );
    }

    {
        Ec ec( 0, system_category() );
        TEST_NOT_FAILED( ec );

        ec.assign( 1, generic_category() );
        TEST_FAILED( ec );

        ec = Ec( 0, generic_category() );
        TEST_NOT_FAILED( ec );
    }

    {
        Ec ec( 1, system_category() );
        TEST_FAILED( ec );

        ec.assign( 0, generic_category() );
        TEST_NOT_FAILED( ec );
    }

    {
        Ec ec( 0, http_category() );
        BOOST_TEST( ec.failed() );

        ec.assign( 200, http_category() );
        BOOST_TEST( !ec.failed() );

        ec = Ec( 404, http_category() );
        BOOST_TEST( ec.failed() );
    }

}

int main()
{
    BOOST_TEST( !generic_category().failed( 0 ) );
    BOOST_TEST( generic_category().failed( 7 ) );

    BOOST_TEST( !system_category().failed( 0 ) );
    BOOST_TEST( system_category().failed( 7 ) );

    BOOST_TEST( http_category().failed( 0 ) );
    BOOST_TEST( !http_category().failed( 200 ) );
    BOOST_TEST( http_category().failed( 404 ) );

    test<error_code>();
    test<error_condition>();

    {
        error_condition ec( errc::success );
        TEST_NOT_FAILED( ec );

        ec = errc::address_family_not_supported;
        TEST_FAILED( ec );
    }

    {
        error_condition ec( errc::address_family_not_supported );
        TEST_FAILED( ec );

        ec = errc::success;
        TEST_NOT_FAILED( ec );
    }

    {
        error_code ec( make_error_code( errc::success ) );
        TEST_NOT_FAILED( ec );

        ec = make_error_code( errc::address_family_not_supported );
        TEST_FAILED( ec );
    }

    {
        error_code ec( make_error_code( errc::address_family_not_supported ) );
        TEST_FAILED( ec );

        ec = make_error_code( errc::success );
        TEST_NOT_FAILED( ec );
    }

    return boost::report_errors();
}
