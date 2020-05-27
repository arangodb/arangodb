// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/test/minimal.hpp>

#include <sstream>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


namespace user {

    struct number
    {
        explicit operator double() const { return value; }

        double value;
    };

    number naxpy(number a, number x, number y)
    {
        return number{a.value * x.value + y.value + 10.0};
    }

    struct tag_type
    {};

    inline number tag_function(double a, double b) { return number{a + b}; }

    struct eval_xform_tag
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::call>, tag_type, number a, double b)
        {
            return tag_function(a.value, b);
        }

        int operator()(
            yap::expr_tag<yap::expr_kind::call>, tag_type, double a, double b)
        {
            return 42;
        }

        char const * operator()() { return "42"; }
    };

    struct empty_xform
    {};

    struct eval_xform_expr
    {
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::call,
                                  bh::tuple<
                                      ref<term<user::tag_type>>,
                                      term<user::number>,
                                      term<int>>> const & expr)
        {
            using namespace boost::hana::literals;
            return tag_function(
                (double)yap::value(expr.elements[1_c]).value,
                (double)yap::value(expr.elements[2_c]));
        }

        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::call,
                                  bh::tuple<
                                      ref<term<user::tag_type>>,
                                      ref<term<user::number>>,
                                      term<int>>> const & expr)
        {
            using namespace boost::hana::literals;
            return tag_function(
                (double)yap::value(expr.elements[1_c]).value,
                (double)yap::value(expr.elements[2_c]));
        }
    };

    struct eval_xform_both
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::call>,
            tag_type,
            user::number a,
            double b)
        {
            return tag_function(a.value, b);
        }

        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::call,
                                  bh::tuple<
                                      ref<term<user::tag_type>>,
                                      term<user::number>,
                                      term<int>>> const & expr)
        {
            using namespace boost::hana::literals;
            throw std::logic_error("Oops!  Picked the wrong overload!");
            return tag_function(
                (double)yap::value(expr.elements[1_c]).value,
                (double)yap::value(expr.elements[2_c]));
        }

        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::call,
                                  bh::tuple<
                                      ref<term<user::tag_type>>,
                                      ref<term<user::number>>,
                                      term<int>>> const & expr)
        {
            using namespace boost::hana::literals;
            throw std::logic_error("Oops!  Picked the wrong overload!");
            return tag_function(
                (double)yap::value(expr.elements[1_c]).value,
                (double)yap::value(expr.elements[2_c]));
        }
    };
}


int test_main(int, char * [])
{
    {
        using namespace boost::yap::literals;

        {
            auto plus = yap::make_terminal(user::tag_type{});
            auto expr = plus(user::number{13}, 1);

            {
                transform(expr, user::empty_xform{});
            }

            {
                user::number result = transform(expr, user::eval_xform_tag{});
                BOOST_CHECK(result.value == 14);
            }

            {
                user::number result = transform(expr, user::eval_xform_expr{});
                BOOST_CHECK(result.value == 14);
            }

            {
                user::number result = transform(expr, user::eval_xform_both{});
                BOOST_CHECK(result.value == 14);
            }
        }

        {
            auto plus = yap::make_terminal(user::tag_type{});
            auto thirteen = yap::make_terminal(user::number{13});
            auto expr = plus(thirteen, 1);

            {
                user::number result = transform(expr, user::eval_xform_tag{});
                BOOST_CHECK(result.value == 14);
            }

            {
                user::number result = transform(expr, user::eval_xform_expr{});
                BOOST_CHECK(result.value == 14);
            }

            {
                user::number result = transform(expr, user::eval_xform_both{});
                BOOST_CHECK(result.value == 14);
            }
        }

        {
            term<user::number> a{{1.0}};
            term<user::number> x{{42.0}};
            term<user::number> y{{3.0}};
            auto n = yap::make_terminal(user::naxpy);

            auto expr = n(a, x, y);
            {
                user::number result = evaluate(expr);
                BOOST_CHECK(result.value == 55);
            }
        }
    }

    return 0;
}
