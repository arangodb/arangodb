/*==============================================================================
    Copyright (c) 2005 Peter Dimov
    Copyright (c) 2005-2010 Joel de Guzman
    Copyright (c) 2010 Thomas Heller
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/config.hpp>

#if defined(BOOST_MSVC)
#pragma warning(disable: 4786)  // identifier truncated in debug info
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4711)  // function selected for automatic inline expansion
#pragma warning(disable: 4514)  // unreferenced inline removed
#endif

#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/bind.hpp>
#include <boost/detail/lightweight_test.hpp>

#if defined(BOOST_MSVC) && (BOOST_MSVC < 1300)
#pragma warning(push, 3)
#endif

#include <iostream>
#include <boost/function.hpp>

#if defined(BOOST_MSVC) && (BOOST_MSVC < 1300)
#pragma warning(pop)
#endif

int f1( int x )
{
    return x;
}

int f2( int x, int y )
{
    return x + y;
}

struct X
{
  mutable int n;

  X() : n(0) {}

    int f0() { n += f1(17); return n; }
    int g0() const { n += g1(17); return n; }


    int f1(int a1) {  return a1; }
    int g1(int a1) const { return a1; }
};

struct Y
{
    int m;
};

namespace phx = boost::phoenix;
using phx::placeholders::arg1;
using phx::placeholders::arg2;
using boost::phoenix::ref;

void member_test()
{
    Y y = { 17041 };
    Y * py = &y;

    BOOST_TEST( boost::bind( &Y::m, _1 )( y ) == 17041 );
    BOOST_TEST( boost::bind( &Y::m, _1 )( py ) == 17041 );

    BOOST_TEST( phx::bind( &Y::m, _1 )( y ) == 17041 );
    BOOST_TEST( phx::bind( &Y::m, _1 )( py ) == 17041 );

    BOOST_TEST( phx::bind( &Y::m, arg1 )( y ) == 17041 );
    BOOST_TEST( phx::bind( &Y::m, arg1 )( py ) == 17041 );

    BOOST_TEST( boost::bind( &Y::m, y )() == 17041 );
    BOOST_TEST( boost::bind( &Y::m, py )() == 17041 );
    //BOOST_TEST( boost::bind( &Y::m, ref(y) )() == 17041 );

    BOOST_TEST( phx::bind( &Y::m, y )() == 17041 );
    BOOST_TEST( phx::bind( &Y::m, py )() == 17041 );
    BOOST_TEST( phx::bind( &Y::m, ref(y) )() == 17041 );

    return;
}

void member_function_test()
{

    X x;

    // 0

    BOOST_TEST(boost::bind(&X::f0, &x )() == 17);
    //boost::bind(&X::f0, ref(x) )(); boost::bind does not work with phx::ref.

    BOOST_TEST(boost::bind(&X::g0, &x )() == 34);
    BOOST_TEST(boost::bind(&X::g0, x )()  == 51);
    //boost::bind(&X::g0, ref(x) )();

    BOOST_TEST(phx::bind(&X::f0, &x )()   == 51);
    BOOST_TEST(phx::bind(&X::f0, ref(x) )() == 68);

    BOOST_TEST(phx::bind(&X::g0, &x )()   == 85);
    BOOST_TEST(phx::bind(&X::g0, x )()    == 102);
    BOOST_TEST(phx::bind(&X::g0, ref(x) )() == 102);

    return;
}

int main()
{

  boost::function<int (int)> fun1_f1(boost::bind ( &f1, _1) );
  boost::function<int (int)> fun2_f1(  phx::bind ( &f1, _1) );
  boost::function<int (int)> fun3_f1(  phx::bind ( &f1, arg1) );

  BOOST_TEST( fun1_f1(1) == 1 );
  BOOST_TEST( fun2_f1(2) == 2 );
  BOOST_TEST( fun3_f1(3) == 3 );

  boost::function<int (int, int)> fun1_f2(boost::bind ( &f2, _1, _2) );
  boost::function<int (int, int)> fun2_f2(  phx::bind ( &f2, _1, _2) );
  boost::function<int (int, int)> fun3_f2(  phx::bind ( &f2, arg1, arg2) );

  BOOST_TEST( fun1_f2(1,2) == 3 );
  BOOST_TEST( fun2_f2(2,3) == 5 );
  BOOST_TEST( fun3_f2(3,4) == 7 );
 
  member_function_test();
  member_test();
  return boost::report_errors();
}
