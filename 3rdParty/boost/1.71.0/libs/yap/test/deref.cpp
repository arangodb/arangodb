// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/mpl/assert.hpp>

#include <boost/test/minimal.hpp>


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


int test_main(int, char * [])
{
    {
        term<double> unity = {{1.0}};
        using plus_expr_type = yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int>>>;

        plus_expr_type plus_expr = unity + term<int>{{1}};

        {
            ref<term<double> &> ref = bh::front(plus_expr.elements);
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::deref(std::move(ref))),
                              term<double> &>));
            BOOST_CHECK(yap::value(ref) == 1.0);
        }

        {
            ref<term<double> &> ref = bh::front(plus_expr.elements);
            BOOST_MPL_ASSERT(
                (std::is_same<decltype(yap::deref(ref)), term<double> &>));
            BOOST_CHECK(yap::value(ref) == 1.0);
        }

        {
            ref<term<double> &> const ref = bh::front(plus_expr.elements);
            BOOST_MPL_ASSERT(
                (std::is_same<decltype(yap::deref(ref)), term<double> &>));
            BOOST_CHECK(yap::value(ref) == 1.0);
        }

        {
            term<double> const unity = {{1.0}};
            yap::expression<
                yap::expr_kind::plus,
                bh::tuple<ref<term<double> const &>, term<int>>>
                plus_expr = unity + term<int>{{1}};

            {
                ref<term<double> const &> ref = bh::front(plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::deref(std::move(ref))),
                                  term<double> const &>));
                BOOST_CHECK(yap::value(ref) == 1.0);
            }

            {
                ref<term<double> const &> ref = bh::front(plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::deref(ref)),
                                  term<double> const &>));
                BOOST_CHECK(yap::value(ref) == 1.0);
            }

            {
                ref<term<double> const &> const ref =
                    bh::front(plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::deref(ref)),
                                  term<double> const &>));
                BOOST_CHECK(yap::value(ref) == 1.0);
            }
        }
    }

    {
        user_term<double> unity = {{1.0}};
        using plus_expr_type = user_expr<
            yap::expr_kind::plus,
            bh::tuple<user_ref<user_term<double> &>, user_term<int>>>;

        plus_expr_type plus_expr = unity + user_term<int>{{1}};

        {
            user_ref<user_term<double> &> ref = bh::front(plus_expr.elements);
            BOOST_MPL_ASSERT((std::is_same<
                              decltype(yap::deref(std::move(ref))),
                              user_term<double> &>));
            BOOST_CHECK(yap::value(ref) == 1.0);
        }

        {
            user_ref<user_term<double> &> ref = bh::front(plus_expr.elements);
            BOOST_MPL_ASSERT(
                (std::is_same<decltype(yap::deref(ref)), user_term<double> &>));
            BOOST_CHECK(yap::value(ref) == 1.0);
        }

        {
            user_ref<user_term<double> &> const ref =
                bh::front(plus_expr.elements);
            BOOST_MPL_ASSERT(
                (std::is_same<decltype(yap::deref(ref)), user_term<double> &>));
            BOOST_CHECK(yap::value(ref) == 1.0);
        }

        {
            user_term<double> const unity = {{1.0}};
            user_expr<
                yap::expr_kind::plus,
                bh::tuple<user_ref<user_term<double> const &>, user_term<int>>>
                plus_expr = unity + user_term<int>{{1}};

            {
                user_ref<user_term<double> const &> ref =
                    bh::front(plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::deref(std::move(ref))),
                                  user_term<double> const &>));
                BOOST_CHECK(yap::value(ref) == 1.0);
            }

            {
                user_ref<user_term<double> const &> ref =
                    bh::front(plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::deref(ref)),
                                  user_term<double> const &>));
                BOOST_CHECK(yap::value(ref) == 1.0);
            }

            {
                user_ref<user_term<double> const &> const ref =
                    bh::front(plus_expr.elements);
                BOOST_MPL_ASSERT((std::is_same<
                                  decltype(yap::deref(ref)),
                                  user_term<double> const &>));
                BOOST_CHECK(yap::value(ref) == 1.0);
            }
        }
    }

    return 0;
}
