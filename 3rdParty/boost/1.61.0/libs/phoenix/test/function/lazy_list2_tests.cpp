////////////////////////////////////////////////////////////////////////////
// lazy_list2_tests.cpp
//
// more tests on list<T>
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
    using namespace phx;

    list<int> l0;
    list<int> l1 = list_with<>()(1,2,3,4,5);
    list<int> l2 = all_but_last(l1)();
 
    BOOST_TEST(null(l0)());
    BOOST_TEST(head(l1)()       == 1);
    BOOST_TEST(head(tail(l1))() == 2);
    BOOST_TEST(last(l1)()       == 5);
    BOOST_TEST(last(l2)()       == 4);
    BOOST_TEST(head(drop(2,l2))() == 3);
   
    return boost::report_errors();
}
