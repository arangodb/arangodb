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

    X( X const& r ): v_( r.v_ ) { ++instances; }

    X& operator=( X const& ) = default;

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

    Y& operator=( Y const& ) = default;

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

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> r;
        result<int> r2( 1 );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> r;
        result<int> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r;
        result<int> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r;
        result<int> r2( 1 );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r;
        result<int> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2( 1 );

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> const r;
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> const r;
        result<X> r2( 1 );

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> const r;
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    // value lhs

    {
        result<int> r( 0 );
        result<int> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> r( 0 );
        result<int> r2( 1 );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> r( 0 );
        result<int> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r( 0 );
        result<int> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r( 0 );
        result<int> r2( 1 );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r( 0 );
        result<int> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2( 2 );

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> const r( 1 );
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> const r( 1 );
        result<X> r2( 2 );

        BOOST_TEST_EQ( X::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> const r( 1 );
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    // error lhs

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2( 1 );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const r( ec );
        result<int> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const r( ec );
        result<int> r2( 1 );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const r( ec );
        result<int> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> r( ec );
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> r( ec );
        result<X> r2( 1 );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> r( ec );
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 0 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> const r( ec );
        result<X> r2;

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> const r( ec );
        result<X> r2( 1 );

        BOOST_TEST_EQ( X::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<X> const r( ec );
        result<X> r2( ENOENT, generic_category() );

        BOOST_TEST_EQ( X::instances, 0 );

        r2 = r;

        BOOST_TEST_EQ( X::instances, 0 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    //

    BOOST_TEST_EQ( Y::instances, 0 );

    {
        result<std::string, Y> r( 1 );
        result<std::string, Y> r2( 2 );

        BOOST_TEST_EQ( Y::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( Y::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( Y::instances, 0 );

    {
        result<std::string, Y> r( 1 );
        result<std::string, Y> r2( "str" );

        BOOST_TEST_EQ( Y::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( Y::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( Y::instances, 0 );

    {
        result<std::string, Y> const r( 1 );
        result<std::string, Y> r2( 2 );

        BOOST_TEST_EQ( Y::instances, 2 );

        r2 = r;

        BOOST_TEST_EQ( Y::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( Y::instances, 0 );

    {
        result<std::string, Y> const r( 1 );
        result<std::string, Y> r2( "str" );

        BOOST_TEST_EQ( Y::instances, 1 );

        r2 = r;

        BOOST_TEST_EQ( Y::instances, 2 );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( Y::instances, 0 );

    // void

    {
        result<void> r;
        result<void> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<void> r;
        result<void> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<void> const r;
        result<void> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<void> const r;
        result<void> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );
        result<void> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );
        result<void> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> const r( ec );
        result<void> r2;

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> const r( ec );
        result<void> r2( ENOENT, generic_category() );

        r2 = r;

        BOOST_TEST_EQ( r, r2 );
    }

    return boost::report_errors();
}
