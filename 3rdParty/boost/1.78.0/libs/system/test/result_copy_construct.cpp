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
        result<int> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r;
        result<int> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r;
        result<X> r2( r );

        BOOST_TEST_EQ( r, r2 );
        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<int> r( 1 );
        result<int> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<int> const r( 1 );
        result<int> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> r( 1 );
        result<X> r2( r );

        BOOST_TEST_EQ( r, r2 );
        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<X> const r( 1 );
        result<X> r2( r );

        BOOST_TEST_EQ( r, r2 );
        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> r( ec );
        result<int> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<int> const r( ec );
        result<int> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<std::string, X> r( 1 );
        result<std::string, X> r2( r );

        BOOST_TEST_EQ( r, r2 );
        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<std::string, X> const r( 1 );
        result<std::string, X> r2( r );

        BOOST_TEST_EQ( r, r2 );
        BOOST_TEST_EQ( X::instances, 2 );
    }

    BOOST_TEST_EQ( X::instances, 0 );

    {
        result<void> r;
        result<void> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        result<void> const r;
        result<void> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> r( ec );
        result<void> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    {
        auto ec = make_error_code( errc::invalid_argument );

        result<void> const r( ec );
        result<void> r2( r );

        BOOST_TEST_EQ( r, r2 );
    }

    return boost::report_errors();
}
