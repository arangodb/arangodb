////////////////////////////////////////////////////////////////////////////
// lazy_operator_tests.cpp
//
// lazy operator tests
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
#include <boost/phoenix/function/lazy_operator.hpp>


int main()
{
    using boost::phoenix::plus;
    using boost::phoenix::minus;
    using boost::phoenix::arg_names::arg1;
    using boost::phoenix::arg_names::arg2;

    int a = 123;
    int b = 256;
  
    BOOST_TEST(plus(arg1, arg2)(a, b)    == a+b);
    BOOST_TEST(plus(arg1, arg2, 3)(a, b) == a+b+3);
    BOOST_TEST(plus(arg1, b)(a)          == a+b);
    BOOST_TEST(plus(a, arg2)(a,b)        == a+b);
    BOOST_TEST(plus(a, arg1)(b)          == a+b);
    BOOST_TEST(plus(a, b)()              == a+b);
    BOOST_TEST(minus(a, b)()             == a-b);
    BOOST_TEST(plus(minus(a, b),b)()             == a);
    BOOST_TEST(plus(minus(arg1, b),b)(a)         == a);
    BOOST_TEST(plus(minus(arg1, arg2),b)(a,b)    == a);
    BOOST_TEST(plus(minus(arg1, arg2),arg2)(a,b) ==a);

    return boost::report_errors();
}
