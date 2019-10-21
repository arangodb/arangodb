// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


template<yap::expr_kind Kind, typename Tuple>
struct expr
{
    static yap::expr_kind const kind = Kind;
    Tuple elements;

    BOOST_YAP_USER_ASSIGN_OPERATOR(expr, ::expr);
};


static_assert(yap::detail::copy_or_move<int, int const &>::value, "");
static_assert(yap::detail::copy_or_move<int, int &>::value, "");
static_assert(yap::detail::copy_or_move<int, int &&>::value, "");
static_assert(!yap::detail::copy_or_move<int, int const &&>::value, "");
static_assert(!yap::detail::copy_or_move<int, int>::value, "");


void compile_user_macros()
{
    using namespace boost::hana::literals;

    expr<yap::expr_kind::negate, bh::tuple<int>> negation1;
    negation1.elements[0_c] = 1;
    expr<yap::expr_kind::negate, bh::tuple<int>> negation2;
    negation2.elements[0_c] = 2;

    // Normal-rules assignment.
    negation2 = negation1;
    assert(negation2.elements[0_c] == 1);

    negation2.elements[0_c] = 2;

    // Normal-rules move assignment.
    negation2 = std::move(negation1);
    assert(negation2.elements[0_c] == 1);

    // Produce a new expression via BOOST_YAP_USER_ASSIGN_OPERATOR.
    auto expr = negation1 = 2;
    (void)expr;
}
