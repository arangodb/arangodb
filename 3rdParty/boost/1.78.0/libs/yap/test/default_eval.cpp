// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/core/lightweight_test.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


int main()
{
    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::minus,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity - std::move(i);
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::minus,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr_1 = unity + std::move(expr);

        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<double> &>>>
            unevaluated_expr_2 = unity + unity;

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            unevaluated_expr_3 = unity + const_unity;

        {
            double result = evaluate(unity);
            BOOST_TEST(result == 1);
        }

        {
            double result = evaluate(expr);
            BOOST_TEST(result == -41);
        }

        {
            double result = evaluate(unevaluated_expr_1);
            BOOST_TEST(result == -40);
        }

        {
            double result = evaluate(unevaluated_expr_2);
            BOOST_TEST(result == 2);
        }

        {
            double result = evaluate(unevaluated_expr_3);
            BOOST_TEST(result == 2);
        }

        {
            double result = evaluate(unity, 5, 6, 7);
            BOOST_TEST(result == 1);
        }

        {
            double result = evaluate(expr);
            BOOST_TEST(result == -41);
        }

        {
            double result = evaluate(unevaluated_expr_1, std::string("15"));
            BOOST_TEST(result == -40);
        }

        {
            double result = evaluate(unevaluated_expr_2, std::string("15"));
            BOOST_TEST(result == 2);
        }

        {
            double result = evaluate(unevaluated_expr_3, std::string("15"));
            BOOST_TEST(result == 2);
        }
    }

    return boost::report_errors();
}
