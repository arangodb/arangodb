// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<long long I>
using place_term =
    boost::yap::terminal<boost::yap::expression, boost::yap::placeholder<I>>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


void compile_placeholders()
{
    using namespace boost::yap::literals;

    {
        place_term<1> p1 = 1_p;
        (void)p1;
    }

    {
        place_term<1> p1 = 1_p;
        term<double> unity{1.0};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<place_term<1> &>, ref<term<double> &>>>
            expr = p1 + unity;
        (void)expr;
    }

    {
        place_term<1> p1 = 1_p;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<place_term<1> &>, place_term<2>>>
            expr = p1 + 2_p;
        (void)expr;
    }
}
