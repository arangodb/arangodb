//  Implicit conversions between compatible reference wrappers
//
//  Copyright 2020 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/ref.hpp>
#include <boost/core/lightweight_test.hpp>

struct X
{
};

struct Y: public X
{
};

void f1( boost::reference_wrapper<X> r, Y * p )
{
    BOOST_TEST_EQ( r.get_pointer(), p );
}

void f2( boost::reference_wrapper<int const> r, int * p )
{
    BOOST_TEST_EQ( r.get_pointer(), p );
}

int main()
{
    Y y;
    f1( boost::ref(y), &y );

    int i = 0;
    f2( boost::ref(i), &i );

    return boost::report_errors();
}
