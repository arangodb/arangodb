//////////////////////////////////////////////////////////////////
//
// lazy_ptr_tests.cpp
//
// Tests for ptr_to_fun, ptr_to_fun0 and ptr_to_mem_fun.
//
//
/*=============================================================================
    Copyright (c) 2000-2003 Brian McNamara and Yannis Smaragdakis
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <iostream>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/phoenix/function/lazy_prelude.hpp>

#include <boost/detail/lightweight_test.hpp>

using namespace boost::phoenix;

using std::cout;
using std::endl;

namespace example {

  int footle()
  {
    return 0;
  }

  int foobar(int x)
  {
    return 2*x;
  }

  int foxy(int x,int y)
  {
    return x*y;
  }

  int foxyz(int x,int y,int z)
  {
    return x*y + z;
  }

  int fwxyz(int w,int x,int y,int z)
  {
    return w + x*y + z;
  }

}

struct O {
   int aa;
   O( int a ) : aa(a) {}
   int cf( int x ) const { return x+1; }
   int f( int x ) { return ++aa + x; }
   int a() const { return aa; }
};


int main() {
   using boost::phoenix::arg_names::arg1;
   using boost::phoenix::arg_names::arg2;
   using boost::phoenix::arg_names::arg3;
   using boost::phoenix::arg_names::arg4;

   BOOST_TEST( ptr_to_fun0(&example::footle)()()     == 0 );
   BOOST_TEST( ptr_to_fun(&example::foobar)(arg1)(1) == 2 );
   BOOST_TEST( ptr_to_fun(&example::foxy)(arg1,arg2)(2,3) == 6 );
   BOOST_TEST( ptr_to_fun(&example::foxyz)(arg1,arg2,arg3)(2,3,4) == 10 );
   BOOST_TEST( ptr_to_fun(&example::fwxyz)(arg1,arg2,arg3,arg4)(1,2,3,4) == 11);

   O o(1);
   BOOST_TEST( ptr_to_mem_fun( &O::a  )( &o )()    == 1 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( &o, 1 )() == 2 );
   BOOST_TEST( ptr_to_mem_fun( &O::f  )( &o, 1 )() == 3 );
   BOOST_TEST( ptr_to_mem_fun( &O::f  )( &o, 1 )() == 4 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( &o, 1 )() == 2 );

   O oo(1);
   BOOST_TEST( ptr_to_mem_fun( &O::a  )( arg1 )( &oo )  == 1 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( &oo, arg1 )(1) == 2 );
   BOOST_TEST( ptr_to_mem_fun( &O::f  )( &oo, arg1 )(1) == 3 );
   BOOST_TEST( ptr_to_mem_fun( &O::f  )( &oo, arg1 )(1) == 4 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( &oo, arg1 )(1) == 2 );

   const O p(1);
   BOOST_TEST( ptr_to_mem_fun( &O::a  )( &p )()    == 1 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( &p, 1 )() == 2 );
 
   boost::shared_ptr<O> r( new O(3) );
   BOOST_TEST( ptr_to_mem_fun( &O::a  )( r )()    == 3 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( r, 1 )() == 2 );
   BOOST_TEST( ptr_to_mem_fun( &O::f  )( r, 1 )() == 5 );
   BOOST_TEST( ptr_to_mem_fun( &O::f  )( r, 1 )() == 6 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( r, 1 )() == 2 );

   boost::shared_ptr<const O> s( new O(3) );
   BOOST_TEST( ptr_to_mem_fun( &O::a  )( s )()    == 3 );
   BOOST_TEST( ptr_to_mem_fun( &O::cf )( s, 1 )() == 2 );

   return boost::report_errors();
}
