// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>

using namespace boost::system;

struct X
{
    int v_;

    explicit X( int v ): v_( v ) {}

    X( X const& ) = delete;
    X& operator=( X const& ) = delete;
};

struct Y
{
};

struct E
{
};

BOOST_NORETURN void throw_exception_from_error( Y const & )
{
    throw E();
}

int main()
{
    {
        result<int> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_EQ( r.value(), 0 );
        BOOST_TEST_EQ( *r, 0 );

        BOOST_TEST_EQ( r.operator->(), &*r );
    }

    {
        result<int> const r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_EQ( r.value(), 0 );
        BOOST_TEST_EQ( *r, 0 );

        BOOST_TEST_EQ( r.operator->(), &*r );
    }

    {
        BOOST_TEST( result<int>().has_value() );
        BOOST_TEST( !result<int>().has_error() );

        BOOST_TEST( result<int>() );
        BOOST_TEST_NOT( !result<int>() );

        BOOST_TEST_EQ( result<int>().value(), 0 );
        BOOST_TEST_EQ( *result<int>(), 0 );

        BOOST_TEST( result<int>().operator->() != 0 );
    }

    {
        result<int> r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_EQ( r.value(), 1 );
        BOOST_TEST_EQ( *r, 1 );

        BOOST_TEST_EQ( r.operator->(), &*r );
    }

    {
        result<int> const r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_EQ( r.value(), 1 );
        BOOST_TEST_EQ( *r, 1 );

        BOOST_TEST_EQ( r.operator->(), &*r );
    }

    {
        BOOST_TEST( result<int>( 1 ).has_value() );
        BOOST_TEST( !result<int>( 1 ).has_error() );

        BOOST_TEST( result<int>( 1 ) );
        BOOST_TEST_NOT( !result<int>( 1 ) );

        BOOST_TEST_EQ( result<int>( 1 ).value(), 1 );
        BOOST_TEST_EQ( *result<int>( 1 ), 1 );

        BOOST_TEST( result<int>( 1 ).operator->() != 0 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_NOT( r );
        BOOST_TEST( !r );

        BOOST_TEST_THROWS( r.value(), system_error );

        BOOST_TEST_EQ( r.operator->(), static_cast<int*>(0) );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_NOT( r );
        BOOST_TEST( !r );

        BOOST_TEST_THROWS( r.value(), system_error );

        BOOST_TEST_EQ( r.operator->(), static_cast<int*>(0) );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST( !result<int>( ec ).has_value() );
        BOOST_TEST( result<int>( ec ).has_error() );

        BOOST_TEST_NOT( result<int>( ec ) );
        BOOST_TEST( !result<int>( ec ) );

        BOOST_TEST_THROWS( result<int>( ec ).value(), system_error );

        BOOST_TEST_EQ( result<int>( ec ).operator->(), static_cast<int*>(0) );
    }

    {
        result<X> r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_EQ( r.value().v_, 1 );
        BOOST_TEST_EQ( (*r).v_, 1 );
        BOOST_TEST_EQ( r->v_, 1 );

        BOOST_TEST_EQ( r.operator->(), &*r );
    }

    {
        result<X> const r( 1 );

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_EQ( r.value().v_, 1 );
        BOOST_TEST_EQ( (*r).v_, 1 );
        BOOST_TEST_EQ( r->v_, 1 );

        BOOST_TEST_EQ( r.operator->(), &*r );
    }

    {
        BOOST_TEST( result<X>( 1 ).has_value() );
        BOOST_TEST( !result<X>( 1 ).has_error() );

        BOOST_TEST( result<X>( 1 ) );
        BOOST_TEST_NOT( !result<X>( 1 ) );

        BOOST_TEST_EQ( result<X>( 1 ).value().v_, 1 );
        BOOST_TEST_EQ( (*result<X>( 1 )).v_, 1 );
        BOOST_TEST_EQ( result<X>( 1 )->v_, 1 );
    }

    {
        auto ec = Y();

        result<X, Y> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_NOT( r );
        BOOST_TEST( !r );

        BOOST_TEST_THROWS( r.value(), E );

        BOOST_TEST_EQ( r.operator->(), static_cast<X*>(0) );
    }

    {
        auto ec = Y();

        result<X, Y> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_NOT( r );
        BOOST_TEST( !r );

        BOOST_TEST_THROWS( r.value(), E );

        BOOST_TEST_EQ( r.operator->(), static_cast<X*>(0) );
    }

    {
        auto ec = Y();

        BOOST_TEST(( !result<X, Y>( ec ).has_value() ));
        BOOST_TEST(( result<X, Y>( ec ).has_error() ));

        BOOST_TEST_NOT(( result<X, Y>( ec ) ));
        BOOST_TEST(( !result<X, Y>( ec ) ));

        BOOST_TEST_THROWS( (result<X, Y>( ec ).value()), E );

        BOOST_TEST_EQ( (result<X, Y>( ec ).operator->()), static_cast<X*>(0) );
    }

    {
        result<void> r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_NO_THROW( r.value() );

        BOOST_TEST( r.operator->() != 0 );
    }

    {
        result<void> const r;

        BOOST_TEST( r.has_value() );
        BOOST_TEST( !r.has_error() );

        BOOST_TEST( r );
        BOOST_TEST_NOT( !r );

        BOOST_TEST_NO_THROW( r.value() );

        BOOST_TEST( r.operator->() != 0 );
    }

    {
        BOOST_TEST( result<void>().has_value() );
        BOOST_TEST( !result<void>().has_error() );

        BOOST_TEST( result<void>() );
        BOOST_TEST_NOT( !result<void>() );

        BOOST_TEST_NO_THROW( result<void>().value() );

        BOOST_TEST( result<void>().operator->() != 0 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_NOT( r );
        BOOST_TEST( !r );

        BOOST_TEST_THROWS( r.value(), system_error );

        BOOST_TEST_EQ( r.operator->(), static_cast<void*>(0) );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> const r( ec );

        BOOST_TEST( !r.has_value() );
        BOOST_TEST( r.has_error() );

        BOOST_TEST_NOT( r );
        BOOST_TEST( !r );

        BOOST_TEST_THROWS( r.value(), system_error );

        BOOST_TEST_EQ( r.operator->(), static_cast<void*>(0) );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        BOOST_TEST( !result<void>( ec ).has_value() );
        BOOST_TEST( result<void>( ec ).has_error() );

        BOOST_TEST_NOT( result<void>( ec ) );
        BOOST_TEST( !result<void>( ec ) );

        BOOST_TEST_THROWS( result<void>( ec ).value(), system_error );

        BOOST_TEST_EQ( result<void>( ec ).operator->(), static_cast<void*>(0) );
    }

    return boost::report_errors();
}
