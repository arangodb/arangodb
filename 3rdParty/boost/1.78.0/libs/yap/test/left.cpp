// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/mpl/assert.hpp>

#include <boost/core/lightweight_test.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


template<boost::yap::expr_kind Kind, typename Tuple>
struct user_expr
{
    static boost::yap::expr_kind const kind = Kind;

    Tuple elements;
};

BOOST_YAP_USER_BINARY_OPERATOR(plus, user_expr, user_expr)

template<typename T>
using user_term = boost::yap::terminal<user_expr, T>;

template<typename T>
using user_ref = boost::yap::expression_ref<user_expr, T>;


int main()
{
    {
        term<double> unity = {{1.0}};
        using plus_expr_type = yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int>>>;

        {
            plus_expr_type plus_expr = unity + term<int>{{1}};
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::left(std::move(plus_expr))),
                              ref<term<double> &> &&>));
        }

        {
            plus_expr_type plus_expr = unity + term<int>{{1}};
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::left(plus_expr)),
                              ref<term<double> &> &>));
        }

        {
            plus_expr_type const plus_expr = unity + term<int>{{1}};
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::left(plus_expr)),
                              ref<term<double> &> const &>));
        }

        {
            term<double> const unity = {{1.0}};
            using plus_expr_type = yap::expression<
                yap::expr_kind::plus,
                bh::tuple<ref<term<double> const &>, term<int>>>;

            {
                plus_expr_type plus_expr = unity + term<int>{{1}};
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(std::move(plus_expr))),
                                  ref<term<double> const &> &&>));
            }

            {
                plus_expr_type plus_expr = unity + term<int>{{1}};
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr)),
                                  ref<term<double> const &> &>));
            }

            {
                plus_expr_type const plus_expr = unity + term<int>{{1}};
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr)),
                                  ref<term<double> const &> const &>));
            }
        }

        {
            term<double> unity = {{1.0}};
            using plus_expr_type = yap::expression<
                yap::expr_kind::plus,
                bh::tuple<ref<term<double> &>, term<int>>>;
            plus_expr_type plus_expr = unity + term<int>{{1}};

            using plus_plus_expr_type = yap::expression<
                yap::expr_kind::plus,
                bh::tuple<ref<plus_expr_type &>, term<int>>>;

            {
                plus_plus_expr_type plus_plus_expr = plus_expr + term<int>{{1}};
                ref<plus_expr_type &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(std::move(plus_expr_ref))),
                                  ref<term<double> &> &>));
            }

            {
                plus_plus_expr_type plus_plus_expr = plus_expr + term<int>{{1}};
                ref<plus_expr_type &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  ref<term<double> &> &>));
            }

            {
                plus_plus_expr_type plus_plus_expr = plus_expr + term<int>{{1}};
                ref<plus_expr_type &> const plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  ref<term<double> &> &>));
            }
        }

        {
            term<double> unity = {{1.0}};
            using plus_expr_type = yap::expression<
                yap::expr_kind::plus,
                bh::tuple<ref<term<double> &>, term<int>>>;
            plus_expr_type const plus_expr = unity + term<int>{{1}};

            using plus_plus_expr_type = yap::expression<
                yap::expr_kind::plus,
                bh::tuple<ref<plus_expr_type const &>, term<int>>>;

            {
                plus_plus_expr_type plus_plus_expr = plus_expr + term<int>{{1}};
                ref<plus_expr_type const &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(std::move(plus_expr_ref))),
                                  ref<term<double> &> const &>));
            }

            {
                plus_plus_expr_type plus_plus_expr = plus_expr + term<int>{{1}};
                ref<plus_expr_type const &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  ref<term<double> &> const &>));
            }

            {
                plus_plus_expr_type plus_plus_expr = plus_expr + term<int>{{1}};
                ref<plus_expr_type const &> const plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  ref<term<double> &> const &>));
            }
        }
    }

    {
        user_term<double> unity = {{1.0}};
        using plus_expr_type = user_expr<
            yap::expr_kind::plus,
            bh::tuple<user_ref<user_term<double> &>, user_term<int>>>;

        {
            plus_expr_type plus_expr = unity + user_term<int>{{1}};
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::left(std::move(plus_expr))),
                              user_ref<user_term<double> &> &&>));
        }

        {
            plus_expr_type plus_expr = unity + user_term<int>{{1}};
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::left(plus_expr)),
                              user_ref<user_term<double> &> &>));
        }

        {
            plus_expr_type const plus_expr = unity + user_term<int>{{1}};
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::left(plus_expr)),
                              user_ref<user_term<double> &> const &>));
        }

        {
            user_term<double> const unity = {{1.0}};
            using plus_expr_type = user_expr<
                yap::expr_kind::plus,
                bh::tuple<user_ref<user_term<double> const &>, user_term<int>>>;

            {
                plus_expr_type plus_expr = unity + user_term<int>{{1}};
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(std::move(plus_expr))),
                                  user_ref<user_term<double> const &> &&>));
            }

            {
                plus_expr_type plus_expr = unity + user_term<int>{{1}};
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr)),
                                  user_ref<user_term<double> const &> &>));
            }

            {
                plus_expr_type const plus_expr = unity + user_term<int>{{1}};
                BOOST_MPL_ASSERT(
                    (std::is_same<
                        decltype(yap::left(plus_expr)),
                        user_ref<user_term<double> const &> const &>));
            }
        }

        {
            user_term<double> unity = {{1.0}};
            using plus_expr_type = user_expr<
                yap::expr_kind::plus,
                bh::tuple<user_ref<user_term<double> &>, user_term<int>>>;
            plus_expr_type plus_expr = unity + user_term<int>{{1}};

            using plus_plus_expr_type = user_expr<
                yap::expr_kind::plus,
                bh::tuple<user_ref<plus_expr_type &>, user_term<int>>>;

            {
                plus_plus_expr_type plus_plus_expr =
                    plus_expr + user_term<int>{{1}};
                user_ref<plus_expr_type &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(std::move(plus_expr_ref))),
                                  user_ref<user_term<double> &> &>));
            }

            {
                plus_plus_expr_type plus_plus_expr =
                    plus_expr + user_term<int>{{1}};
                user_ref<plus_expr_type &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  user_ref<user_term<double> &> &>));
            }

            {
                plus_plus_expr_type plus_plus_expr =
                    plus_expr + user_term<int>{{1}};
                user_ref<plus_expr_type &> const plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  user_ref<user_term<double> &> &>));
            }
        }

        {
            user_term<double> unity = {{1.0}};
            using plus_expr_type = user_expr<
                yap::expr_kind::plus,
                bh::tuple<user_ref<user_term<double> &>, user_term<int>>>;
            plus_expr_type const plus_expr = unity + user_term<int>{{1}};

            using plus_plus_expr_type = user_expr<
                yap::expr_kind::plus,
                bh::tuple<user_ref<plus_expr_type const &>, user_term<int>>>;

            {
                plus_plus_expr_type plus_plus_expr =
                    plus_expr + user_term<int>{{1}};
                user_ref<plus_expr_type const &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(std::move(plus_expr_ref))),
                                  user_ref<user_term<double> &> const &>));
            }

            {
                plus_plus_expr_type plus_plus_expr =
                    plus_expr + user_term<int>{{1}};
                user_ref<plus_expr_type const &> plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  user_ref<user_term<double> &> const &>));
            }

            {
                plus_plus_expr_type plus_plus_expr =
                    plus_expr + user_term<int>{{1}};
                user_ref<plus_expr_type const &> const plus_expr_ref =
                    bh::front(plus_plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::left(plus_expr_ref)),
                                  user_ref<user_term<double> &> const &>));
            }
        }
    }

#ifndef _MSC_VER // Tsk, tsk.
    {
        using term_t = term<int>;
        constexpr auto expr = term_t{13} + term_t{42};
        constexpr auto result1 = expr.left().value();
        constexpr auto result2 = yap::value(left(expr));
        (void)result1;
        (void)result2;
    }
#endif

    return boost::report_errors();
}
