// Copyright 2017, 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/result.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iosfwd>
#include <cerrno>

using namespace boost::system;

struct X
{
    static int instances;

    int v_;

    explicit X( int v ): v_( v ) { ++instances; }

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
    {
        result<int> r1( 1 );
        result<int> r2( 2 );

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_NE( r1, r2 );
    }

    {
        result<int> r1( 1, generic_category() );
        result<int> r2( 2, generic_category() );

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_NE( r1, r2 );
    }

    {
        result<int> r1( 1 );
        result<int> r2( 2, generic_category() );

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_NE( r1, r2 );
    }

    {
        result<X, Y> r1( in_place_value, 1 );
        result<X, Y> r2( in_place_value, 2 );

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_NE( r1, r2 );
    }

    {
        result<X, Y> r1( in_place_error, 1 );
        result<X, Y> r2( in_place_error, 2 );

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_NE( r1, r2 );
    }

    {
        result<X, Y> r1( in_place_value, 1 );
        result<X, Y> r2( in_place_error, 2 );

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_NE( r1, r2 );
    }

    {
        result<void> r1;
        result<void> r2;

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_EQ( r1, r2 );
    }

    {
        result<void> r1( 1, generic_category() );
        result<void> r2( 2, generic_category() );

        BOOST_TEST_EQ( r1, r1 );
        BOOST_TEST_NE( r1, r2 );
    }

    return boost::report_errors();
}
