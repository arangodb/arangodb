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

namespace boost { namespace phoenix
{
    struct for_each_impl
    {
        template <typename Sig>
        struct result;

        template <typename This, typename C, typename F>
        struct result<This(C,F)>
        {
            typedef void type;
        };

        template <typename C, typename F>
        void operator()(C& c, F f) const
        {
            std::for_each(c.begin(), c.end(), f);
        }
    };

    function<for_each_impl> const for_each = for_each_impl();

    struct push_back_impl
    {
        template <typename Sig>
        struct result;

        template <typename This, typename C, typename T>
        struct result<This(C,T)>
        {
            typedef void type;
        };

        template <typename C, typename T>
        void operator()(C& c, T& x) const
        {
            c.push_back(x);
        }
    };

    function<push_back_impl> const push_back = push_back_impl();
}}

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
    //    using boost::phoenix::local_names::_b;
    using boost::phoenix::placeholders::arg1;

    {
        using boost::phoenix::for_each;
        //#if (!defined(BOOST_MSVC) || (BOOST_MSVC < 1800))
        int init[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        std::vector<int> v(init, init+10);

        int x = 0;
        for_each(_1, lambda(_a = _2)[_a += _1])(v, x);
        BOOST_TEST(x == 55);
        //#endif
    }

    {
        using boost::phoenix::for_each;
        using boost::phoenix::push_back;

        //#if (!defined(BOOST_MSVC) || (BOOST_MSVC < 1800))
        int x = 10;
        std::vector<std::vector<int> > v(10);
        for_each(_1, lambda(_a = _2)[push_back(_1, _a)])(v, x);

        int y = 0;
        for_each(arg1, lambda[ref(y) += _1[0]])(v);
        BOOST_TEST(y == 100);
        //#endif
    }

    return boost::report_errors();
}

