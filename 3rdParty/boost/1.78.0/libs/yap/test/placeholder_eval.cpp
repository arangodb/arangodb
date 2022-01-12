// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/core/lightweight_test.hpp>

#include <sstream>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<long long I>
using place_term =
    boost::yap::terminal<boost::yap::expression, boost::yap::placeholder<I>>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


int main()
{
    {
        using namespace boost::yap::literals;

        place_term<3> p3 = 3_p;
        int i_ = 42;
        term<int> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<place_term<3> &>, term<int>>>
            expr = p3 + std::move(i);
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<
                ref<place_term<3> &>,
                yap::expression<
                    yap::expr_kind::plus,
                    bh::tuple<ref<place_term<3> &>, term<int>>>>>
            unevaluated_expr = p3 + std::move(expr);

        {
            double result = evaluate(p3, 5, 6, 7);
            BOOST_TEST(result == 7);
        }

        {
            double result = evaluate(expr, std::string("15"), 3, 1);
            BOOST_TEST(result == 43);
        }

        {
            double result = evaluate(unevaluated_expr, std::string("15"), 2, 3);
            BOOST_TEST(result == 48);
        }
    }

    return boost::report_errors();
}
