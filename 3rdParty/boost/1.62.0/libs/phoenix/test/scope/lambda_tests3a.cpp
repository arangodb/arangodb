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
#include <boost/phoenix/bind.hpp>
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

    /*
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
    */
    {
       {
            // $$$ Fixme. This should not be failing $$$
            //int x = (let(_a = lambda[val(1)])[_a])()();
            //BOOST_TEST(x == 1);
       }

       {
//#if defined(BOOST_GCC_VERSION) && (BOOST_GCC_VERSION >= 50000) && __OPTIMIZE__
#if defined(__OPTIMIZE__) && __OPTIMIZE__
            int x = (let(_a = _1)[bind(_a)])(lambda[val(1)]());
#else
            int x = (let(_a = lambda[val(1)])[bind(_a)])();
#endif
            BOOST_TEST(x == 1);
         // Take this out too, I am not sure about this.
       }
    }
    /*
    {
        int i = 0;
        int j = 2;
        BOOST_TEST(lambda[let(_a = _1)[_a = _2]]()(i, j) == j);
        BOOST_TEST(i == j);
    }
    */

    return boost::report_errors();
}
