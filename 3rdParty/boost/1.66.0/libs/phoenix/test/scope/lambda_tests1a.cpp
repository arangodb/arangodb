/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2014      John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

#include <typeinfo>

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/function.hpp>
//#include <boost/phoenix/bind.hpp>
#include <boost/phoenix/scope.hpp>

int
main()
{
    using boost::phoenix::lambda;
    using boost::phoenix::let;
    using boost::phoenix::ref;
    using boost::phoenix::val;
    using boost::phoenix::arg_names::_1;
    using boost::phoenix::arg_names::_2;
    using boost::phoenix::local_names::_a;
    using boost::phoenix::local_names::_b;
    //    using boost::phoenix::placeholders::arg1;

    {
        int x = 1;
        int y = lambda[_1]()(x);
        BOOST_TEST(x == y);
    }

    {
        int x = 1, y = 10;
        BOOST_TEST(
            (_1 + lambda[_1 + 2])(x)(y) == 1+10+2
        );
        BOOST_TEST(
            (_1 + lambda[-_1])(x)(y) == 1+-10
        );
    }

    {
        int x = 1, y = 10, z = 13;
        BOOST_TEST(
            lambda(_a = _1, _b = _2)
            [
                _1 + _a + _b
            ]
            (x, z)(y) == x + y + z
        );
    }

    {
        int x = 4;
        int y = 5;
        lambda(_a = _1)[_a = 555](x)();
        BOOST_TEST(x == 555);
        (void)y;
    }

    return boost::report_errors();
}
