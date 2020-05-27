/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>

#include <boost/phoenix/core/limits.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/tuple.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/function.hpp>
#include <boost/phoenix/fusion.hpp>
#include <boost/phoenix/scope.hpp>

#include <typeinfo>

namespace fusion = boost::fusion;
namespace mpl = boost::mpl;

int
main()
{
    using boost::phoenix::let;
    using boost::phoenix::val;
    using boost::phoenix::arg_names::_1;
    using boost::phoenix::arg_names::_2;
    using boost::phoenix::local_names::_a;
    using boost::phoenix::local_names::_b;

    {
        // show that we can return a local from an outer scope
        int y = 0;
#if defined(BOOST_GCC_VERSION) && (BOOST_GCC_VERSION >= 50000) && __OPTIMIZE__
        int x = (let(_a = _2)[let(_b = _1)[ _a ]])(y,1);
#else
        int x = (let(_a = 1)[let(_b = _1)[ _a ]])(y);
#endif
        BOOST_TEST(x == 1);
    }
    {
        // show that we can return a local from an inner scope
        int y = 1;
        int x = (let(_a = 0)[let(_b = _1)[ _b ]])(y);

        BOOST_TEST(x == 1);
    }
    {
        // show that we can return a local from an outer scope
        //int y = 0;
#if defined(BOOST_GCC_VERSION) && (BOOST_GCC_VERSION >= 50000) && __OPTIMIZE__
        int x = (let(_a = 1)[let(_b = _a)[ _a ]])();
#else
        int x = (let(_a = _1)[let(_b = _a)[ _a ]])(1);
#endif
        BOOST_TEST(x == 1);
    }
    {
        // show that we can return a local from an inner scope
        //int y = 0;
#if defined(BOOST_GCC_VERSION) && (BOOST_GCC_VERSION >= 50000) && __OPTIMIZE__
        int x = (let(_a = _1)[let(_b = _a)[ _b ]])(1);
#else
        int x = (let(_a = 1)[let(_b = _a)[ _b ]])();
#endif
        BOOST_TEST(x == 1);
    }
    {
        // show that we can return a local from an outer scope
        int y = 1;
        int x = (let(_a = _1)[let(_b = _a)[ _a ]])(y);

        BOOST_TEST(x == 1);
    }
    {
        // show that we can return a local from an inner scope
        int y = 1;
        int x = (let(_a = _1)[let(_b = _a)[ _b ]])(y);

        BOOST_TEST(x == 1);
    }

    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // Be very careful. Some of these cases give a silly answer
    // with clang 3.4 with C++03 and work for C++11.
    // gcc 4.8.2 seems O.K. both ways. Oh dear.
    //++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    /*  {
    int y = 0;
    int x = (let(_a = 1, _b = 2)[let(_b = _a)[ _a ]])(y);
    //std::cout << x << " P1A "; //clang - empty memory
        BOOST_TEST(x == 1);
  }
  {
    int y = 0;
    int x = (let(_a = 1, _b = 2)[let(_b = _a)[ _b ]])(y);
    //std::cout << x << " P1B "; //clang - 42 value- one step better
        BOOST_TEST(x == 1);
  }
  {
    int y = 0;
    int x = (let(_a = val(1), _b = val(2))[let(_b = _a)[ _a ]])(y);
    //std::cout << x << " P2A "; //clang - 42 value - one step better
        BOOST_TEST(x == 1);
  }
  {
    int y = 0;
    int x = (let(_a = val(1), _b = val(2))[let(_b = _a)[ _b ]])(y);
    //std::cout << x << " P2B "; //clang - 42 value - one step better
        BOOST_TEST(x == 1);
  }
  {
    int y = 1;
    int x = (let(_a = _1, _b = val(2))[let(_b = _a)[ _a ]])(y);
    //std::cout << x << " P3 "; //clang - OK - one step better still
        BOOST_TEST(x == 1);
  }

  {
    int y = 0;
    int x = (let(_a = 1, _b = 2)[let(_b = _1)[ _a ]])(y);
    //    std::cout << x << " Q "; // clang 4201472
        BOOST_TEST(x == 1);
  }
    */
    
    return boost::report_errors();
}

