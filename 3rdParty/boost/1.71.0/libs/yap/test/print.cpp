// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>
#include <boost/yap/print.hpp>

#include <boost/test/minimal.hpp>

#include <sstream>
#include <regex>


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

struct thing
{};

std::string fix_tti(std::string s)
{
    // msvc: remove struct/class prefixes
    static const std::regex estruct("(struct|class) ");
    s = std::regex_replace(s, estruct, "");

    // gcc/clang: strip integral literals suffixes
    static const std::regex eint("(\\d)u?l{0,2}");
    s = std::regex_replace(s, eint, "$1");

    return s;
}

int test_main(int, char * [])
{
    {
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::terminal) == std::string("term"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::unary_plus) == std::string("+"));
        BOOST_CHECK(yap::op_string(yap::expr_kind::negate) == std::string("-"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::dereference) == std::string("*"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::complement) == std::string("~"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::address_of) == std::string("&"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::logical_not) == std::string("!"));

        BOOST_CHECK(yap::op_string(yap::expr_kind::pre_inc) == std::string("++"));
        BOOST_CHECK(yap::op_string(yap::expr_kind::pre_dec) == std::string("--"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::post_inc) == std::string("++(int)"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::post_dec) == std::string("--(int)"));

        BOOST_CHECK(
            yap::op_string(yap::expr_kind::shift_left) == std::string("<<"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::shift_right) == std::string(">>"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::multiplies) == std::string("*"));
        BOOST_CHECK(yap::op_string(yap::expr_kind::divides) == std::string("/"));
        BOOST_CHECK(yap::op_string(yap::expr_kind::modulus) == std::string("%"));

        BOOST_CHECK(
            yap::op_string(yap::expr_kind::multiplies_assign) ==
            std::string("*="));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::divides_assign) ==
            std::string("/="));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::modulus_assign) ==
            std::string("%="));

        BOOST_CHECK(
            yap::op_string(yap::expr_kind::plus_assign) == std::string("+="));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::minus_assign) == std::string("-="));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::bitwise_and_assign) ==
            std::string("&="));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::bitwise_or_assign) ==
            std::string("|="));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::bitwise_xor_assign) ==
            std::string("^="));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind::subscript) == std::string("[]"));
        BOOST_CHECK(yap::op_string(yap::expr_kind::if_else) == std::string("?:"));
        BOOST_CHECK(yap::op_string(yap::expr_kind::call) == std::string("()"));
        BOOST_CHECK(
            yap::op_string(yap::expr_kind(-1)) ==
            std::string("** ERROR: UNKNOWN OPERATOR! **"));
    }

    {
        user_term<double> unity{1.0};
        int i_ = 42;
        user_term<int &&> i{std::move(i_)};
        user_expr<
            yap::expr_kind::plus,
            bh::tuple<user_ref<user_term<double> &>, user_term<int &&>>>
            expr = unity + std::move(i);
        user_expr<
            yap::expr_kind::plus,
            bh::tuple<
                user_ref<user_term<double> &>,
                user_expr<
                    yap::expr_kind::plus,
                    bh::tuple<
                        user_ref<user_term<double> &>,
                        user_term<int &&>>>>>
            unevaluated_expr = unity + std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    expr<+>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        user_term<thing> a_thing{bh::make_tuple(thing{})};

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        user_term<double> const const_unity{1.0};
        user_expr<
            yap::expr_kind::plus,
            bh::tuple<
                user_ref<user_term<double> &>,
                user_ref<user_term<double> const &>>>
            nonconst_plus_const = unity + const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_plus_const);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        auto nonconst_plus_nonconst_plus_const = unity + nonconst_plus_const;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_plus_nonconst_plus_const);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    expr<+> &
        term<double>[=1] &
        term<double>[=1] const &
)");
        }

        auto const const_nonconst_plus_const = nonconst_plus_const;
        auto nonconst_plus_nonconst_plus_const_2 =
            unity + const_nonconst_plus_const;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_plus_nonconst_plus_const_2);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    expr<+> const &
        term<double>[=1] &
        term<double>[=1] const &
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity + std::move(i);
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::plus,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity + std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    expr<+>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_plus_const = unity + const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_plus_const);
            BOOST_CHECK(oss.str() == R"(expr<+>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::minus,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity - std::move(i);
        yap::expression<
            yap::expr_kind::minus,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::minus,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity - std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<->
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<->
    term<double>[=1] &
    expr<->
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::minus,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_minus_const = unity - const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_minus_const);
            BOOST_CHECK(oss.str() == R"(expr<->
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::less,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity < std::move(i);
        yap::expression<
            yap::expr_kind::less,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::less,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity < std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<<>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<<>
    term<double>[=1] &
    expr<<>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::less,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_less_const = unity < const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_less_const);
            BOOST_CHECK(oss.str() == R"(expr<<>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::greater,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity > std::move(i);
        yap::expression<
            yap::expr_kind::greater,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::greater,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity > std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<>>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<>>
    term<double>[=1] &
    expr<>>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::greater,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_greater_const = unity > const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_greater_const);
            BOOST_CHECK(oss.str() == R"(expr<>>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::less_equal,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity <= std::move(i);
        yap::expression<
            yap::expr_kind::less_equal,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::less_equal,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity <= std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<<=>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<<=>
    term<double>[=1] &
    expr<<=>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::less_equal,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_less_equal_const = unity <= const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_less_equal_const);
            BOOST_CHECK(oss.str() == R"(expr<<=>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::greater_equal,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity >= std::move(i);
        yap::expression<
            yap::expr_kind::greater_equal,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::greater_equal,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity >= std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<>=>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<>=>
    term<double>[=1] &
    expr<>=>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::greater_equal,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_greater_equal_const = unity >= const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_greater_equal_const);
            BOOST_CHECK(oss.str() == R"(expr<>=>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::equal_to,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity == std::move(i);
        yap::expression<
            yap::expr_kind::equal_to,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::equal_to,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity == std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<==>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<==>
    term<double>[=1] &
    expr<==>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::equal_to,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_equal_to_const = unity == const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_equal_to_const);
            BOOST_CHECK(oss.str() == R"(expr<==>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::not_equal_to,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity != std::move(i);
        yap::expression<
            yap::expr_kind::not_equal_to,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::not_equal_to,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity != std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<!=>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<!=>
    term<double>[=1] &
    expr<!=>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::not_equal_to,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_not_equal_to_const = unity != const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_not_equal_to_const);
            BOOST_CHECK(oss.str() == R"(expr<!=>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::logical_or,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity || std::move(i);
        yap::expression<
            yap::expr_kind::logical_or,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::logical_or,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity || std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<||>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<||>
    term<double>[=1] &
    expr<||>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::logical_or,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_logical_or_const = unity || const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_logical_or_const);
            BOOST_CHECK(oss.str() == R"(expr<||>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::logical_and,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity && std::move(i);
        yap::expression<
            yap::expr_kind::logical_and,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::logical_and,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity && std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<&&>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<&&>
    term<double>[=1] &
    expr<&&>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::logical_and,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_logical_and_const = unity && const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_logical_and_const);
            BOOST_CHECK(oss.str() == R"(expr<&&>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::bitwise_and,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity & std::move(i);
        yap::expression<
            yap::expr_kind::bitwise_and,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::bitwise_and,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity & std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<&>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<&>
    term<double>[=1] &
    expr<&>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::bitwise_and,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_bitwise_and_const = unity & const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_bitwise_and_const);
            BOOST_CHECK(oss.str() == R"(expr<&>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::bitwise_or,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity | std::move(i);
        yap::expression<
            yap::expr_kind::bitwise_or,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::bitwise_or,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity | std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<|>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<|>
    term<double>[=1] &
    expr<|>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::bitwise_or,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_bitwise_or_const = unity | const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_bitwise_or_const);
            BOOST_CHECK(oss.str() == R"(expr<|>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::bitwise_xor,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity ^ std::move(i);
        yap::expression<
            yap::expr_kind::bitwise_xor,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::bitwise_xor,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity ^ std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<^>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<^>
    term<double>[=1] &
    expr<^>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::bitwise_xor,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_bitwise_xor_const = unity ^ const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_bitwise_xor_const);
            BOOST_CHECK(oss.str() == R"(expr<^>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::comma,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = (unity, std::move(i));
        yap::expression<
            yap::expr_kind::comma,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::comma,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = (unity, std::move(expr));

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<,>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<,>
    term<double>[=1] &
    expr<,>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::comma,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_comma_const = (unity, const_unity);

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_comma_const);
            BOOST_CHECK(oss.str() == R"(expr<,>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::mem_ptr,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity->*std::move(i);
        yap::expression<
            yap::expr_kind::mem_ptr,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::mem_ptr,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity->*std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<->*>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<->*>
    term<double>[=1] &
    expr<->*>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::mem_ptr,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_mem_ptr_const = unity->*const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_mem_ptr_const);
            BOOST_CHECK(oss.str() == R"(expr<->*>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::assign,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity = std::move(i);
        yap::expression<
            yap::expr_kind::assign,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::assign,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity = std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<=>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<=>
    term<double>[=1] &
    expr<=>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::shift_left_assign,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity <<= std::move(i);
        yap::expression<
            yap::expr_kind::shift_left_assign,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::shift_left_assign,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity <<= std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<<<=>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<<<=>
    term<double>[=1] &
    expr<<<=>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::shift_left_assign,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_shift_left_assign_const = unity <<= const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_shift_left_assign_const);
            BOOST_CHECK(oss.str() == R"(expr<<<=>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::shift_right_assign,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            expr = unity >>= std::move(i);
        yap::expression<
            yap::expr_kind::shift_right_assign,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::shift_right_assign,
                    bh::tuple<ref<term<double> &>, term<int &&>>>>>
            unevaluated_expr = unity >>= std::move(expr);

        {
            std::ostringstream oss;
            yap::print(oss, unity);
            BOOST_CHECK(oss.str() == R"(term<double>[=1]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, expr);
            BOOST_CHECK(oss.str() == R"(expr<>>=>
    term<double>[=1] &
    term<int &&>[=42]
)");
        }

        {
            std::ostringstream oss;
            yap::print(oss, unevaluated_expr);
            BOOST_CHECK(oss.str() == R"(expr<>>=>
    term<double>[=1] &
    expr<>>=>
        term<double>[=1] &
        term<int &&>[=42]
)");
        }

        term<thing> a_thing(thing{});

        {
            std::ostringstream oss;
            yap::print(oss, a_thing);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<thing>[=<<unprintable-value>>]
)");
        }

        term<double> const const_unity{1.0};
        yap::expression<
            yap::expr_kind::shift_right_assign,
            bh::tuple<ref<term<double> &>, ref<term<double> const &>>>
            nonconst_shift_right_assign_const = unity >>= const_unity;

        {
            std::ostringstream oss;
            yap::print(oss, nonconst_shift_right_assign_const);
            BOOST_CHECK(oss.str() == R"(expr<>>=>
    term<double>[=1] &
    term<double>[=1] const &
)");
        }

        {
            using namespace yap::literals;
            std::ostringstream oss;
            yap::print(oss, 1_p);
            BOOST_CHECK(fix_tti(oss.str()) == R"(term<boost::yap::placeholder<1>>[=1]
)");
        }
    }

    return 0;
}
