// Boost checked_delete test program

// Copyright Beman Dawes 2001. Copyright 2014 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/core/checked_delete.hpp>
#include <boost/core/lightweight_test.hpp>

struct X
{
    static int instances;

    X()
    {
        ++instances;
    }

    ~X()
    {
        --instances;
    }

private:

    X( X const & );
    X & operator=( X const & );
};

int X::instances = 0;

int main()
{
    BOOST_TEST( X::instances == 0 );

    {
        X * p = new X;

        BOOST_TEST( X::instances == 1 );

        boost::checked_delete( p );

        BOOST_TEST( X::instances == 0 );
    }

    {
        X * p = new X[ 3 ];

        BOOST_TEST( X::instances == 3 );

        boost::checked_array_delete( p );

        BOOST_TEST( X::instances == 0 );
    }

    return boost::report_errors();
}
