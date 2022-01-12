// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <string>

using namespace boost::system;

struct X
{
    int v_;

    explicit X( int v = 0 ): v_( v ) {}

    X( X const& ) = default;
    X& operator=( X const& ) = delete;
};

int main()
{
    {
        result<int> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error(), error_code() );
    }

    {
        result<int> const r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error(), error_code() );
    }

    {
        BOOST_TEST( result<int>().has_value() );
        BOOST_TEST( !result<int>().has_error() );

        BOOST_TEST_EQ( result<int>().error(), error_code() );
    }

    {
        result<int> r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error(), error_code() );
    }

    {
        result<int> const r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error(), error_code() );
    }

    {
        BOOST_TEST( result<int>( 1 ).has_value() );
        BOOST_TEST( !result<int>( 1 ).has_error() );

        BOOST_TEST_EQ( result<int>( 1 ).error(), error_code() );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST( !result<int>( ec ).has_value() );
        BOOST_TEST( result<int>( ec ).has_error() );

        BOOST_TEST_EQ( result<int>( ec ).error(), ec );
    }

    {
        result<std::string, X> r( 1 );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 1 );
    }

    {
        result<std::string, X> const r( 1 );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 1 );
    }

    {
        BOOST_TEST(( !result<std::string, X>( 1 ).has_value() ));
        BOOST_TEST(( result<std::string, X>( 1 ).has_error() ));

        BOOST_TEST_EQ( (result<std::string, X>( 1 ).error().v_), 1 );
    }

    {
        result<std::string, X> r( "s" );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 0 );
    }

    {
        result<std::string, X> const r( "s" );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 0 );
    }

    {
        BOOST_TEST(( result<std::string, X>( "s" ).has_value() ));
        BOOST_TEST(( !result<std::string, X>( "s" ).has_error() ));

        BOOST_TEST_EQ( (result<std::string, X>( "s" ).error().v_), 0 );
    }

    {
        result<void> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error(), error_code() );
    }

    {
        result<void> const r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.error(), error_code() );
    }

    {
        BOOST_TEST( result<void>().has_value() );
        BOOST_TEST( !result<void>().has_error() );

        BOOST_TEST_EQ( result<void>().error(), error_code() );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST( !result<void>( ec ).has_value() );
        BOOST_TEST( result<void>( ec ).has_error() );

        BOOST_TEST_EQ( result<void>( ec ).error(), ec );
    }

    return boost::report_errors();
}
