
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/function.hpp>
#include <boost/core/lightweight_test.hpp>

//

void call_fn_1( boost::function<void()> const & fn );
void call_fn_2( boost::function<void(int)> const & fn );
void call_fn_3( boost::function<void(int, int)> const & fn );

void call_fn_4( boost::function0<void> const & fn );
void call_fn_5( boost::function1<void, int> const & fn );
void call_fn_6( boost::function2<void, int, int> const & fn );

//

static int v;

void f0()
{
    v = -1;
}

void f1( int x )
{
    v = x;
}

void f2( int x, int y )
{
    v = x + y;
}

int main()
{
    v = 0; call_fn_1( f0 ); BOOST_TEST_EQ( v, -1 );
    v = 0; call_fn_2( f1 ); BOOST_TEST_EQ( v, 1 );
    v = 0; call_fn_3( f2 ); BOOST_TEST_EQ( v, 3 );

    v = 0; call_fn_4( f0 ); BOOST_TEST_EQ( v, -1 );
    v = 0; call_fn_5( f1 ); BOOST_TEST_EQ( v, 1 );
    v = 0; call_fn_6( f2 ); BOOST_TEST_EQ( v, 3 );

    return boost::report_errors();
}
