////////////////////////////////////////////////////////////////////////////
// lazy_list_tests.cpp
//
// tests on list<T>
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
    using phx::null;
    using phx::list;
    using phx::head;
    using phx::tail;
    using phx::cons;
    using phx::cat;
    using phx::take;
    using phx::NIL;

    list<int> l0;
    list<int> l1 = cons(1,l0);
    list<int> l2 = cons(2,l1);
    list<int> l3 = cons(3,l2);
    list<int> l4 = cons(4,l3);
    list<int> l5 = cons(5,NIL);
    list<int> l6 = take(2,l4)();
    list<int> l7 = cons(7,take(2,l4));
    list<int> l8 = take(1,take(3,l4))();
  
    BOOST_TEST(null(l0)());
    BOOST_TEST(null(arg1)(l0));
    BOOST_TEST(head(l1)()     == 1);
    BOOST_TEST(head(arg1)(l1) == 1);
    BOOST_TEST(head(tail(l2))()     == 1);
    BOOST_TEST(head(tail(arg1))(l2) == 1);
    BOOST_TEST(head(tail(tail(l3)))()     == 1);
    BOOST_TEST(head(tail(tail(arg1)))(l3) == 1);
    BOOST_TEST(head(tail(tail(l4)))()     == 2);
    BOOST_TEST(head(tail(tail(arg1)))(l4) == 2);
    BOOST_TEST(head(l5)()     == 5);
    BOOST_TEST(head(arg1)(l5) == 5);
    BOOST_TEST(head(tail(l6))()     == 3);
    BOOST_TEST(head(tail(arg1))(l6) == 3);
    BOOST_TEST(head(tail(l7))()     == 4);
    BOOST_TEST(head(tail(arg1))(l7) == 4);
    BOOST_TEST(head(l8)()     == 4);
    BOOST_TEST(head(arg1)(l8) == 4);

    list<int> l9  = cat(l8,take(2,l4));
    list<int> l10 = cat(l8,NIL);
    list<int> l11 = cat(l0,l7);
    list<int> l12 = cat(l10,l8);

    BOOST_TEST(head(tail(l9))() == 4);
    BOOST_TEST(head(l10)()      == 4);
    BOOST_TEST(head(arg1)(l11)  == 7);
    BOOST_TEST(head(l12)()      == 4);

   
    return boost::report_errors();
}
