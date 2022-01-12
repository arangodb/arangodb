// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <iosfwd>
#include <cerrno>

using namespace boost::system;

struct X
{
    static int instances;

    int v_;

    explicit X( int v = 0 ): v_( v ) { ++instances; }

    X( X const& r ) = delete;
    X( X&& r ): v_( r.v_ ) { r.v_ = 0; ++instances; }

    X& operator=( X const& ) = delete;

    X& operator=( X&& r )
    {
        v_ = r.v_;
        r.v_ = 0;

        return *this;
    }

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

struct Y
{
    static int instances;

    int v_;

    explicit Y( int v = 0 ): v_( v ) { ++instances; }

    Y( Y const& r ) noexcept: v_( r.v_ ) { ++instances; }
    Y( Y&& r ) noexcept: v_( r.v_ ) { r.v_ = 0; ++instances; }

    Y& operator=( Y const& ) = default;

    Y& operator=( Y&& r )
    {
        v_ = r.v_;
        r.v_ = 0;

        return *this;
    }

    ~Y() { --instances; }
};

bool operator==( Y const & y1, Y const & y2 )
{
    return y1.v_ == y2.v_;
}

std::ostream& operator<<( std::ostream& os, Y const & y )
{
    os << "Y:" << y.v_;
    return os;
}

int Y::instances = 0;

int main()
{
    // default-initialized lhs

    {
        result<int> r;
        result<int> r2;

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    {
        result<int> r2;

        r2 = result<int>();

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    {
        result<int> r;
        result<int> r2( 1 );

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    {
        result<int> r2( 1 );

        r2 = result<int>();

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    {
        result<int> r;
        result<int> r2( ENOENT, generic_category() );

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    {
        result<int> r2( ENOENT, generic_category() );

        r2 = result<int>();

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 0 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), X() );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2( 1 );

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), X() );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), X() );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    // value lhs

    {
        result<int> r( 1 );
        result<int> r2;

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 1 );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> r( 1 );
        result<int> r2( 2 );

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 1 );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> r( 1 );
        result<int> r2( ENOENT, generic_category() );

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value(), 1 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value().v_, 1 );

        BOOST_TEST_EQ( r.value().v_, 0 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2( 2 );

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value().v_, 1 );

        BOOST_TEST_EQ( r.value().v_, 0 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );

        BOOST_TEST_EQ( r2.value().v_, 1 );

        BOOST_TEST_EQ( r.value().v_, 0 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    // error lhs

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2;

        r2 = std::move( r );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r2;

        r2 = result<int>( ec );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2( 1 );

        r2 = std::move( r );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r2( 1 );

        r2 = result<int>( ec );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2( ENOENT, generic_category() );

        r2 = std::move( r );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r2( ENOENT, generic_category() );

        r2 = result<int>( ec );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> r( ec );
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> r( ec );
        result<X> r2( 1 );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> r( ec );
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 0 );

        r2 = std::move( r );

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    //

    BOOST_TEST_EQ( Y::instances, 0 );

    {
        result<std::string, Y> r( 1 );
        result<std::string, Y> r2( 2 );

        BOOST_TEST_EQ( Y::instances, 2 );

        r2 = std::move( r );

        BOOST_TEST_EQ( Y::instances, 2 );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error().v_, 1 );

        BOOST_TEST_EQ( r.error().v_, 0 );
    }

    BOOST_TEST_EQ( Y::instances, 0 );

    {
        result<std::string, Y> r( 1 );
        result<std::string, Y> r2( "str" );

        BOOST_TEST_EQ( Y::instances, 1 );

        r2 = std::move( r );

        BOOST_TEST_EQ( Y::instances, 2 );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error().v_, 1 );

        BOOST_TEST_EQ( r.error().v_, 0 );
    }

    BOOST_TEST_EQ( Y::instances, 0 );

    {
        result<void> r;
        result<void> r2;

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );
    }

    {
        result<void> r2;

        r2 = result<void>();

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );
    }

    {
        result<void> r;
        result<void> r2( ENOENT, generic_category() );

        r2 = std::move( r );

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );
    }

    {
        result<void> r2( ENOENT, generic_category() );

        r2 = result<void>();

        BOOST_TEST( r2.has_value() );
        BOOST_TEST( !r2.has_error() );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );
        result<void> r2;

        r2 = std::move( r );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r2;

        r2 = result<void>( ec );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );
        result<void> r2( ENOENT, generic_category() );

        r2 = std::move( r );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r2( ENOENT, generic_category() );

        r2 = result<void>( ec );

        BOOST_TEST( !r2.has_value() );
        BOOST_TEST( r2.has_error() );

        BOOST_TEST_EQ( r2.error(), ec );
    }

    return boost::report_errors();
}
