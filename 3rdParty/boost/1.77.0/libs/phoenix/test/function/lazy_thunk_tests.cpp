//////////////////////////////////////////////////////////////////
//
// lazy_thunk_tests.cpp
//
// Tests for thunk functions.
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



int main() {
   using boost::phoenix::arg_names::arg1;
   using boost::phoenix::arg_names::arg2;

   BOOST_TEST( thunk1(inc,1)()()             == 2);
   BOOST_TEST( thunk1(inc,arg1)(1)()         == 2);
   BOOST_TEST( thunk2(plus,1,2)()()          == 3);
   BOOST_TEST( thunk2(plus,arg1,arg2)(1,2)() == 3);

   list<int> l  = enum_from_to(1,5);
   list<int> l4 = take(4,l)();
   BOOST_TEST( foldl(plus,0,l4)()              == 10);
   BOOST_TEST( thunk3(foldl,plus,0,l4)()()     == 10);
   BOOST_TEST( thunk3(foldl,plus,arg1,l4)(0)() == 10);

   return boost::report_errors();
}
