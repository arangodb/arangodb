// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>

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
    X( X&& r ): v_( r.v_ ) { r.v_ = 0; ++instances; }

    X& operator=( X const& ) = delete;

    ~X() { --instances; }
};

bool operator==( X const & x1, X const & x2 )
{
    return x1.v_ == x2.v_;
}

std::ostream& operator<<( std::ostream& os, X const & x )
{
    os << "X:" << x.v_;
    return os;
}

int X::instances = 0;

int main()
{
    {
        result<int> r;
        result<int> r2( std::move( r ) );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    {
        result<int> r2( result<int>{} );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2( std::move( r ) );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), X() );

        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r2( result<X>{} );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), X() );

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<int> r( 1 );
        result<int> r2( std::move( r ) );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.value(), 1 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 1 );
    }

    {
        result<int> r2( result<int>( 1 ) );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2( std::move( r ) );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST_EQ( r.value().v_, 0 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value().v_, 1 );

        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r2( result<X>( 1 ) );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value().v_, 1 );

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2( std::move( r ) );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r2( result<int>{ ec } );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<std::string, X> r( 1 );
        result<std::string, X> r2( std::move( r ) );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_EQ( r.error().v_, 0 );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error().v_, 1 );

        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<std::string, X> r2( result<std::string, X>( 1 ) );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error().v_, 1 );

        BOOST_TEST_EQ( X::instances, 1 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<void> r;
        result<void> r2( std::move( r ) );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );
    }

    {
        result<void> r2( result<void>{} );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );
        result<void> r2( std::move( r ) );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r2( result<void>{ ec } );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    return boost::report_errors();
}
