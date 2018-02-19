////////////////////////////////////////////////////////////////////////////
// lazy_compose_tests.cpp
//
// tests on compose
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

    BOOST_TEST(compose(id,id)(arg1)(1)   == 1);
    BOOST_TEST(compose(id,id)(1)()       == 1);
    BOOST_TEST(compose(inc,inc)(arg1)(1) == 3);
    BOOST_TEST(compose(inc,plus)(arg1,arg2)(2,3)        == 6);
    BOOST_TEST(compose(plus,inc,dec)(arg1)(1)           == 2);
    BOOST_TEST(compose(plus,plus,minus)(arg1,arg2)(3,1) == 6);


   
    return boost::report_errors();
}
