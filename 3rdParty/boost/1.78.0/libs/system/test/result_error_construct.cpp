// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <string>
#include <cerrno>

using namespace boost::system;

struct X
{
    static int instances;

    int v_;

    X(): v_() { ++instances; }

    explicit X( int v ): v_( v ) { ++instances; }

    X( int v1, int v2 ): v_( v1+v2 ) { ++instances; }
    X( int v1, int v2, int v3 ): v_( v1+v2+v3 ) { ++instances; }

    X( X const& r ): v_( r.v_ ) { ++instances; }

    X& operator=( X const& ) = delete;

    ~X() { --instances; }
};

int X::instances = 0;

int main()
{
    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r = ec;

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        result<int> r( EINVAL, generic_category() );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), std::error_code( EINVAL, generic_category() ) );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<std::error_code> r( in_place_error, ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        result<std::error_code> r( in_place_error, EINVAL, generic_category() );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), std::error_code( EINVAL, generic_category() ) );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<std::string, X> r( 1 );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 1 );

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<int, X> r( 1, 2 );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 1+2 );

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<int, X> r( 1, 2, 3 );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 1+2+3 );

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X, X> r( in_place_error, 1 );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 1 );

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        BOOST_TEST_TRAIT_TRUE((std::is_constructible<result<int>, std::error_code>));
        BOOST_TEST_TRAIT_TRUE((std::is_convertible<std::error_code, result<int>>));

        BOOST_TEST_TRAIT_TRUE((std::is_constructible<result<std::string, X>, int>));
        BOOST_TEST_TRAIT_FALSE((std::is_convertible<int, result<std::string, X>>));

        BOOST_TEST_TRAIT_FALSE((std::is_constructible<result<int, X>, int>));
        BOOST_TEST_TRAIT_FALSE((std::is_convertible<int, result<int, X>>));
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

        result<void> r = ec;

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), ec );
    }

    {
        result<void> r( EINVAL, generic_category() );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error(), std::error_code( EINVAL, generic_category() ) );
    }

    return boost::report_errors();
}
