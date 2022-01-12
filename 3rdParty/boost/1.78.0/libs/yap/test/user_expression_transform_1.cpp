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


namespace user {

    struct number
    {
        double value;

        friend number operator+(number lhs, number rhs)
        {
            return number{lhs.value + rhs.value};
        }

        friend number operator-(number lhs, number rhs)
        {
            return number{lhs.value - rhs.value};
        }

        friend number operator*(number lhs, number rhs)
        {
            return number{lhs.value * rhs.value};
        }

        friend number operator-(number n) { return number{-n.value}; }

        friend bool operator<(number lhs, double rhs)
        {
            return lhs.value < rhs;
        }

        friend bool operator<(double lhs, number rhs)
        {
            return lhs < rhs.value;
        }
    };

    number naxpy(number a, number x, number y)
    {
        return number{a.value * x.value + y.value + 10.0};
    }

    struct empty_xform
    {};

    struct eval_xform_tag
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::terminal>, user::number const & n)
        {
            return n;
        }
    };

    struct eval_xform_expr
    {
        decltype(auto) operator()(term<user::number> const & expr)
        {
            return ::boost::yap::value(expr);
        }
    };

    struct eval_xform_both
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::terminal>, user::number const & n)
        {
            return n;
        }

        decltype(auto) operator()(term<user::number> const & expr)
        {
            throw std::logic_error("Oops!  Picked the wrong overload!");
            return ::boost::yap::value(expr);
        }
    };

    struct plus_to_minus_xform_tag
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::plus>,
            user::number const & lhs,
            user::number const & rhs)
        {
            return yap::make_expression<yap::expr_kind::minus>(
                term<user::number>{lhs}, term<user::number>{rhs});
        }
    };

    struct plus_to_minus_xform_expr
    {
        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<Expr1, Expr2>> const & expr)
        {
            return yap::make_expression<yap::expr_kind::minus>(
                ::boost::yap::left(expr), ::boost::yap::right(expr));
        }
    };

    struct plus_to_minus_xform_both
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::plus>,
            user::number const & lhs,
            user::number const & rhs)
        {
            return yap::make_expression<yap::expr_kind::minus>(
                term<user::number>{lhs}, term<user::number>{rhs});
        }

        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<Expr1, Expr2>> const & expr)
        {
            throw std::logic_error("Oops!  Picked the wrong overload!");
            return yap::make_expression<yap::expr_kind::minus>(
                ::boost::yap::left(expr), ::boost::yap::right(expr));
        }
    };

    struct term_nonterm_xform_tag
    {
        auto operator()(
            yap::expr_tag<yap::expr_kind::terminal>, user::number const & n)
        {
            return yap::make_terminal(n * user::number{2.0});
        }

        auto operator()(
            yap::expr_tag<yap::expr_kind::plus>,
            user::number const & lhs,
            user::number const & rhs)
        {
            return yap::make_expression<yap::expr_kind::minus>(
                yap::transform(::boost::yap::make_terminal(lhs), *this),
                yap::transform(::boost::yap::make_terminal(rhs), *this));
        }
    };

    struct term_nonterm_xform_expr
    {
        decltype(auto) operator()(term<user::number> const & expr)
        {
            return yap::make_terminal(
                ::boost::yap::value(expr) * user::number{2.0});
        }

        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<Expr1, Expr2>> const & expr)
        {
            return yap::make_expression<yap::expr_kind::minus>(
                yap::transform(::boost::yap::left(expr), *this),
                yap::transform(::boost::yap::right(expr), *this));
        }
    };

    struct term_nonterm_xform_both
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::terminal>, user::number const & n)
        {
            return yap::make_terminal(n * user::number{2.0});
        }

        decltype(auto) operator()(term<user::number> const & expr)
        {
            return yap::make_terminal(
                ::boost::yap::value(expr) * user::number{2.0});
        }

        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::plus>,
            user::number const & lhs,
            user::number const & rhs)
        {
            return yap::make_expression<yap::expr_kind::minus>(
                yap::transform(::boost::yap::make_terminal(lhs), *this),
                yap::transform(::boost::yap::make_terminal(rhs), *this));
        }

        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<Expr1, Expr2>> const & expr)
        {
            return yap::make_expression<yap::expr_kind::minus>(
                yap::transform(::boost::yap::left(expr), *this),
                yap::transform(::boost::yap::right(expr), *this));
        }
    };

    struct eval_term_nonterm_xform_tag
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::terminal>, user::number const & n)
        {
            return n * user::number{2.0};
        }

        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::plus>,
            Expr1 const & lhs,
            Expr2 const & rhs)
        {
            return boost::yap::transform(::boost::yap::as_expr(lhs), *this) -
                   boost::yap::transform(::boost::yap::as_expr(rhs), *this);
        }
    };

    struct eval_term_nonterm_xform_expr
    {
        decltype(auto) operator()(term<user::number> const & expr)
        {
            return ::boost::yap::value(expr) * user::number{2.0};
        }

        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<Expr1, Expr2>> const & expr)
        {
            return boost::yap::transform(::boost::yap::left(expr), *this) -
                   boost::yap::transform(::boost::yap::right(expr), *this);
        }
    };

    struct eval_term_nonterm_xform_both
    {
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::terminal>, user::number const & n)
        {
            return n * user::number{2.0};
        }

        decltype(auto) operator()(term<user::number> const & expr)
        {
            return ::boost::yap::value(expr) * user::number{2.0};
        }

        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::plus>,
            Expr1 const & lhs,
            Expr2 const & rhs)
        {
            return boost::yap::transform(::boost::yap::as_expr(lhs), *this) -
                   boost::yap::transform(::boost::yap::as_expr(rhs), *this);
        }

        template<typename Expr1, typename Expr2>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<Expr1, Expr2>> const & expr)
        {
            return boost::yap::transform(::boost::yap::left(expr), *this) -
                   boost::yap::transform(::boost::yap::right(expr), *this);
        }
    };

    decltype(auto)
    naxpy_eager_nontemplate_xform(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<
                                      yap::expression<
                                          yap::expr_kind::multiplies,
                                          bh::tuple<
                                              ref<term<user::number> &>,
                                              ref<term<user::number> &>>>,
                                      ref<term<user::number> &>>> const & expr)
    {
        auto a = evaluate(expr.left().left());
        auto x = evaluate(expr.left().right());
        auto y = evaluate(expr.right());
        return yap::make_terminal(naxpy(a, x, y));
    }

    decltype(auto)
    naxpy_lazy_nontemplate_xform(yap::expression<
                                 yap::expr_kind::plus,
                                 bh::tuple<
                                     yap::expression<
                                         yap::expr_kind::multiplies,
                                         bh::tuple<
                                             ref<term<user::number> &>,
                                             ref<term<user::number> &>>>,
                                     ref<term<user::number> &>>> const & expr)
    {
        decltype(auto) a = expr.left().left().value();
        decltype(auto) x = expr.left().right().value();
        decltype(auto) y = expr.right().value();
        return yap::make_terminal(naxpy)(a, x, y);
    }

    struct naxpy_xform
    {
        template<typename Expr1, typename Expr2, typename Expr3>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::plus,
                                  bh::tuple<
                                      yap::expression<
                                          yap::expr_kind::multiplies,
                                          bh::tuple<Expr1, Expr2>>,
                                      Expr3>> const & expr)
        {
            return yap::make_terminal(naxpy)(
                transform(expr.left().left(), naxpy_xform{}),
                transform(expr.left().right(), naxpy_xform{}),
                transform(expr.right(), naxpy_xform{}));
        }
    };


    // unary transforms

    struct disable_negate_xform_tag
    {
        auto
        operator()(yap::expr_tag<yap::expr_kind::negate>, user::number value)
        {
            return yap::make_terminal(std::move(value));
        }

        template<typename Expr>
        auto
        operator()(yap::expr_tag<yap::expr_kind::negate>, Expr const & expr)
        {
            return expr;
        }
    };

    struct disable_negate_xform_expr
    {
        template<typename Expr>
        decltype(auto) operator()(
            yap::expression<yap::expr_kind::negate, bh::tuple<Expr>> const &
                expr)
        {
            return ::boost::yap::value(expr);
        }
    };

    struct disable_negate_xform_both
    {
        decltype(auto)
        operator()(yap::expr_tag<yap::expr_kind::negate>, user::number value)
        {
            return yap::make_terminal(std::move(value));
        }

        template<typename Expr>
        decltype(auto)
        operator()(yap::expr_tag<yap::expr_kind::negate>, Expr const & expr)
        {
            return expr;
        }

        template<typename Expr>
        decltype(auto) operator()(
            yap::expression<yap::expr_kind::negate, bh::tuple<Expr>> const &
                expr)
        {
            throw std::logic_error("Oops!  Picked the wrong overload!");
            return ::boost::yap::value(expr);
        }
    };


    // ternary transforms

    //[ tag_xform
    struct ternary_to_else_xform_tag
    {
        template<typename Expr>
        decltype(auto) operator()(
            boost::yap::expr_tag<yap::expr_kind::if_else>,
            Expr const & cond,
            user::number then,
            user::number else_)
        {
            return boost::yap::make_terminal(std::move(else_));
        }
    };
    //]

    //[ expr_xform
    struct ternary_to_else_xform_expr
    {
        template<typename Cond, typename Then, typename Else>
        decltype(auto)
        operator()(boost::yap::expression<
                   boost::yap::expr_kind::if_else,
                   boost::hana::tuple<Cond, Then, Else>> const & expr)
        {
            return ::boost::yap::else_(expr);
        }
    };
    //]

    struct ternary_to_else_xform_both
    {
        template<typename Expr>
        decltype(auto) operator()(
            yap::expr_tag<yap::expr_kind::if_else>,
            Expr const & cond,
            user::number then,
            user::number else_)
        {
            return yap::make_terminal(std::move(else_));
        }

        template<typename Cond, typename Then, typename Else>
        decltype(auto) operator()(yap::expression<
                                  yap::expr_kind::if_else,
                                  bh::tuple<Cond, Then, Else>> const & expr)
        {
            throw std::logic_error("Oops!  Picked the wrong overload!");
            return ::boost::yap::else_(expr);
        }
    };
}

auto double_to_float(term<double> expr)
{
    return term<float>{(float)expr.value()};
}

auto check_unique_ptrs_equal_7(term<std::unique_ptr<int>> && expr)
{
    using namespace boost::hana::literals;
    BOOST_TEST(*expr.elements[0_c] == 7);
    return std::move(expr);
}

int main()
{
    {
        term<user::number> a{{1.0}};
        term<user::number> x{{42.0}};
        term<user::number> y{{3.0}};

        {
            auto expr = a;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 1);
            }

            {
                auto transformed_expr = transform(expr, user::empty_xform{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 1);
            }

            {
                auto transformed_expr = transform(expr, user::eval_xform_tag{});
                BOOST_TEST(transformed_expr.value == 1);
            }

            {
                auto transformed_expr =
                    transform(expr, user::eval_xform_expr{});
                BOOST_TEST(transformed_expr.value == 1);
            }

            {
                auto transformed_expr =
                    transform(expr, user::eval_xform_both{});
                BOOST_TEST(transformed_expr.value == 1);
            }
        }

        {
            auto expr = x + y;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::plus_to_minus_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39);
            }

            {
                auto transformed_expr =
                    transform(expr, user::plus_to_minus_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39);
            }

            {
                auto transformed_expr =
                    transform(expr, user::plus_to_minus_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39);
            }
        }

        {
            auto expr = x + user::number{3.0};
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39 * 2);
            }
        }

        {
            auto expr = x + y;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 39 * 2);
            }
        }

        {
            auto expr = (x + y) + user::number{1.0};
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 46);
            }

            {
                // Differs from those below, because it matches terminals, not
                // expressions.
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 40 * 2);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 38 * 2);
            }

            {
                auto transformed_expr =
                    transform(expr, user::term_nonterm_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 38 * 2);
            }
        }

        {
            auto expr = x + user::number{3.0};
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_tag{});
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_expr{});
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_both{});
                BOOST_TEST(result.value == 39 * 2);
            }
        }

        {
            auto expr = x + y;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_tag{});
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_expr{});
                BOOST_TEST(result.value == 39 * 2);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_both{});
                BOOST_TEST(result.value == 39 * 2);
            }
        }

        {
            auto expr = (x + y) + user::number{1.0};
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 46);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_tag{});
                BOOST_TEST(result.value == 38 * 2);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_expr{});
                BOOST_TEST(result.value == 38 * 2);
            }

            {
                user::number result =
                    transform(expr, user::eval_term_nonterm_xform_both{});
                BOOST_TEST(result.value == 38 * 2);
            }
        }

        {
            auto expr = a * x + y;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45);
            }

            auto transformed_expr =
                transform(expr, user::naxpy_eager_nontemplate_xform);
            {
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 55);
            }
        }

        {
            auto expr = a + (a * x + y);
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 46);
            }

            auto transformed_expr =
                transform(expr, user::naxpy_eager_nontemplate_xform);
            {
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 56);
            }
        }

        {
            auto expr = a * x + y;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45);
            }

            auto transformed_expr =
                transform(expr, user::naxpy_lazy_nontemplate_xform);
            {
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 55);
            }
        }

        {
            auto expr = a + (a * x + y);
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 46);
            }

            auto transformed_expr =
                transform(expr, user::naxpy_lazy_nontemplate_xform);
            {
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 56);
            }
        }

        {
            auto expr = (a * x + y) * (a * x + y) + (a * x + y);
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 45 * 45 + 45);
            }

            auto transformed_expr = transform(expr, user::naxpy_xform{});
            {
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 55 * 55 + 55 + 10);
            }
        }
    }

    {
        term<double> unity{1.0};
        term<std::unique_ptr<int>> i{new int{7}};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<std::unique_ptr<int>>>>
            expr_1 = unity + std::move(i);

        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::plus,
                    bh::tuple<
                        ref<term<double> &>,
                        term<std::unique_ptr<int>>>>>>
            expr_2 = unity + std::move(expr_1);

        auto transformed_expr = transform(std::move(expr_2), double_to_float);

        transform(std::move(transformed_expr), check_unique_ptrs_equal_7);
    }

    {
        term<user::number> a{{1.0}};
        term<user::number> x{{42.0}};
        term<user::number> y{{3.0}};

        {
            auto expr = -x;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == -42);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 42);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 42);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 42);
            }
        }

        {
            auto expr = a * -x + y;
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == -39);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 45);
            }
        }

        {
            auto expr = -(x + y);
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == -45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 45);
            }

            {
                auto transformed_expr =
                    transform(expr, user::disable_negate_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 45);
            }
        }
    }

    {
        term<user::number> a{{1.0}};
        term<user::number> x{{42.0}};
        term<user::number> y{{3.0}};

        {
            auto expr = if_else(0 < a, x, y);
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 42);
            }

            {
                auto transformed_expr =
                    transform(expr, user::ternary_to_else_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 3);
            }

            {
                auto transformed_expr =
                    transform(expr, user::ternary_to_else_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 3);
            }

            {
                auto transformed_expr =
                    transform(expr, user::ternary_to_else_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 3);
            }
        }

        {
            auto expr = y * if_else(0 < a, x, y) + user::number{0.0};
            {
                user::number result = evaluate(expr);
                BOOST_TEST(result.value == 126);
            }

            {
                auto transformed_expr =
                    transform(expr, user::ternary_to_else_xform_tag{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 9);
            }

            {
                auto transformed_expr =
                    transform(expr, user::ternary_to_else_xform_expr{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 9);
            }

            {
                auto transformed_expr =
                    transform(expr, user::ternary_to_else_xform_both{});
                user::number result = evaluate(transformed_expr);
                BOOST_TEST(result.value == 9);
            }
        }
    }

    return boost::report_errors();
}
