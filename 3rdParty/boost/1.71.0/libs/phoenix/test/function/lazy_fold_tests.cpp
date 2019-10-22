////////////////////////////////////////////////////////////////////////////
// lazy_fold_tests.cpp
//
// tests on foldl foldr and compose
//
////////////////////////////////////////////////////////////////////////////
/*=============================================================================
    Copyright (c) 2000-2003 Brian McNamara and Yannis Smaragdakis
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix/core/limits.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/function/lazy_prelude.hpp>


int main()
{
    namespace phx = boost::phoenix;
    using boost::phoenix::arg_names::arg1;
    using boost::phoenix::arg_names::arg2;
    using boost::phoenix::arg_names::arg3;
    using namespace phx;

    list<int> l = enum_from(2);
    list<int> evens = filter(even,l);
    list<int> even4 = take(4,evens)();

    BOOST_TEST(foldr(plus,0,even4)()       == 20);
    BOOST_TEST(foldr(multiplies,1,even4)() == 384);
    BOOST_TEST(foldr1(plus,even4)()        == 20 );
    BOOST_TEST(foldl(plus,arg1,even4)(0)   == 20);
    BOOST_TEST(foldl1(plus,even4)()        == 20);
    BOOST_TEST(compose(inc,foldr)(plus,0,even4)()  == 21);
    BOOST_TEST(compose(plus,foldl,foldr)(arg1,arg2,arg3)(plus,0,even4) == 40);
   
    return boost::report_errors();
}
