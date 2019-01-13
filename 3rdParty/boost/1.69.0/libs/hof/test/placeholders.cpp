/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    placeholders.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/placeholders.hpp>
#include "test.hpp"
#include <sstream>

// TODO: Test assign operators

#if BOOST_HOF_HAS_STATIC_TEST_CHECK
#define BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR constexpr
#else
#define BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR
#endif

struct square
{
    template<class T>
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto operator()(T x) const BOOST_HOF_RETURNS(x*x);
};

#define CHECK_DEFAULT_CONSTRUCTION_OP(op, name) \
    static_assert(boost::hof::detail::is_default_constructible<boost::hof::operators::name>::value, "Operator not default constructible");

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
#define PLACEHOLDER_CHECK(...) \
    BOOST_HOF_STATIC_TEST_CHECK(__VA_ARGS__); \
    BOOST_HOF_TEST_CHECK(__VA_ARGS__); \
    static_assert(noexcept(__VA_ARGS__), "noexcept placeholders")
#else
#define PLACEHOLDER_CHECK(...) \
    BOOST_HOF_STATIC_TEST_CHECK(__VA_ARGS__); \
    BOOST_HOF_TEST_CHECK(__VA_ARGS__)
#endif

BOOST_HOF_TEST_CASE()
{
    static_assert(boost::hof::detail::is_default_constructible<boost::hof::placeholder<1>>::value, "Placeholder not default constructible");
    static_assert(boost::hof::detail::is_default_constructible<boost::hof::placeholder<2>>::value, "Placeholder not default constructible");
    static_assert(boost::hof::detail::is_default_constructible<boost::hof::placeholder<3>>::value, "Placeholder not default constructible");

    BOOST_HOF_FOREACH_BINARY_OP(CHECK_DEFAULT_CONSTRUCTION_OP)
    BOOST_HOF_FOREACH_ASSIGN_OP(CHECK_DEFAULT_CONSTRUCTION_OP)
    BOOST_HOF_FOREACH_UNARY_OP(CHECK_DEFAULT_CONSTRUCTION_OP)
}

BOOST_HOF_TEST_CASE()
{
    auto simple_print = boost::hof::reveal(std::ref(std::cout) << boost::hof::_);
    simple_print("Hello");
}

BOOST_HOF_TEST_CASE()
{
    std::stringstream ss;
    auto simple_print = boost::hof::reveal(std::ref(ss) << boost::hof::_);
    simple_print("Hello");
    BOOST_HOF_TEST_CHECK(ss.str() == "Hello");
}

BOOST_HOF_TEST_CASE()
{
    const auto x_square_add = 2 + (4*4);
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_square_add = boost::hof::_1 + boost::hof::lazy(square())(boost::hof::_2);
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_square_add)>::value, "Not copyable");
#endif
    PLACEHOLDER_CHECK(f_square_add(2, 4) == x_square_add);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_invoke_2 = boost::hof::_1(3);
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_invoke_2)>::value, "Not copyable");
#endif
    PLACEHOLDER_CHECK(f_invoke_2(square()) == 9);
}

BOOST_HOF_TEST_CASE()
{
    const auto x_add = 2 + 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_add = boost::hof::_1 + boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_add)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_add)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_add(2, 1) == x_add);

    const auto x_subtract = 2 - 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_subtract = boost::hof::_1 - boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_subtract)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_subtract)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_subtract(2, 1) == x_subtract);

    const auto x_multiply = 2 * 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_multiply = boost::hof::_1 * boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_multiply)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_multiply)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_multiply(2, 1) == x_multiply);

    const auto x_divide = 2 / 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_divide = boost::hof::_1 / boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_divide)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_divide)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_divide(2, 1) == x_divide);

    const auto x_remainder = 2 % 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_remainder = boost::hof::_1 % boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_remainder)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_remainder)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_remainder(2, 1) == x_remainder);

    const auto x_shift_right = 2 >> 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_right = boost::hof::_1 >> boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_right)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_shift_right)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_shift_right(2, 1) == x_shift_right);

    const auto x_shift_left = 2 << 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_left = boost::hof::_1 << boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_left)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_shift_left)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_shift_left(2, 1) == x_shift_left);

    const auto x_greater_than = 2 > 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than = boost::hof::_1 > boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_greater_than)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_greater_than(2, 1) == x_greater_than);

    const auto x_less_than = 2 < 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than = boost::hof::_1 < boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_less_than)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_less_than(2, 1) == x_less_than);

    const auto x_less_than_equal = 2 <= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than_equal = boost::hof::_1 <= boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_less_than_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_less_than_equal(2, 1) == x_less_than_equal);

    const auto x_greater_than_equal = 2 >= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than_equal = boost::hof::_1 >= boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_greater_than_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_greater_than_equal(2, 1) == x_greater_than_equal);

    const auto x_equal = 2 == 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_equal = boost::hof::_1 == boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_equal(2, 1) == x_equal);

    const auto x_not_equal = 2 != 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_not_equal = boost::hof::_1 != boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_not_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_not_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_not_equal(2, 1) == x_not_equal);

    const auto x_bit_and = 2 & 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_and = boost::hof::_1 & boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_and)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_bit_and)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_bit_and(2, 1) == x_bit_and);

    const auto x_xor_ = 2 ^ 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_xor_ = boost::hof::_1 ^ boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_xor_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_xor_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_xor_(2, 1) == x_xor_);

    const auto x_bit_or = 2 | 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_or = boost::hof::_1 | boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_or)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_bit_or)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_bit_or(2, 1) == x_bit_or);

    const auto x_and_ = true && false;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_and_ = boost::hof::_1 && boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_and_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_and_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_and_(true, false) == x_and_);

    const auto x_or_ = true;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_or_ = boost::hof::_1 || boost::hof::_2;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_or_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_or_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_or_(true, false) == x_or_);
}

BOOST_HOF_TEST_CASE()
{

    const auto x_not_ = !false;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_not_ = !boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_not_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_not_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_not_(false) == x_not_);

    const auto x_not_0 = !0;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_not_0 = !boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_not_0)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_not_0)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_not_0(0) == x_not_0);

    const auto x_compl_ = ~2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_compl_ = ~boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_compl_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_compl_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_compl_(2) == x_compl_);

    const auto x_unary_plus = +2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_unary_plus = +boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_unary_plus)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_unary_plus)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_unary_plus(2) == x_unary_plus);

    const auto x_unary_subtract = -2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_unary_subtract = -boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_unary_subtract)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_unary_subtract)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_unary_subtract(2) == x_unary_subtract);

    const auto x_dereference = 2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_dereference = *boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_dereference)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_dereference)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_dereference(&x_dereference) == x_dereference);

    // TODO: Test BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR increment and decrement
#ifndef _MSC_VER
    auto x_increment = 2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_increment = ++boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_increment)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_increment)>::value, "Not default constructible");
    f_increment(x_increment);
    BOOST_HOF_TEST_CHECK(x_increment == 3);

    auto x_decrement = 2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_decrement = --boost::hof::_1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_decrement)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_decrement)>::value, "Not default constructible");
    f_decrement(x_decrement);
    BOOST_HOF_TEST_CHECK(x_decrement == 1);
#endif

    // TODO: Test post increment and decrement
}


BOOST_HOF_TEST_CASE()
{
    const auto x_add = 2 + 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_add = boost::hof::_ + boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_add)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_add)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_add(2, 1) == x_add);

    const auto x_subtract = 2 - 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_subtract = boost::hof::_ - boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_subtract)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_subtract)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_subtract(2, 1) == x_subtract);

    const auto x_multiply = 2 * 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_multiply = boost::hof::_ * boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_multiply)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_multiply)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_multiply(2, 1) == x_multiply);

    const auto x_divide = 2 / 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_divide = boost::hof::_ / boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_divide)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_divide)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_divide(2, 1) == x_divide);

    const auto x_remainder = 2 % 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_remainder = boost::hof::_ % boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_remainder)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_remainder)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_remainder(2, 1) == x_remainder);

    const auto x_shift_right = 2 >> 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_right = boost::hof::_ >> boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_right)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_shift_right)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_shift_right(2, 1) == x_shift_right);

    const auto x_shift_left = 2 << 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_left = boost::hof::_ << boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_left)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_shift_left)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_shift_left(2, 1) == x_shift_left);

    const auto x_greater_than = 2 > 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than = boost::hof::_ > boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_greater_than)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_greater_than(2, 1) == x_greater_than);

    const auto x_less_than = 2 < 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than = boost::hof::_ < boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_less_than)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_less_than(2, 1) == x_less_than);

    const auto x_less_than_equal = 2 <= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than_equal = boost::hof::_ <= boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_less_than_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_less_than_equal(2, 1) == x_less_than_equal);

    const auto x_greater_than_equal = 2 >= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than_equal = boost::hof::_ >= boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_greater_than_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_greater_than_equal(2, 1) == x_greater_than_equal);

    const auto x_equal = 2 == 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_equal = boost::hof::_ == boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_equal(2, 1) == x_equal);

    const auto x_not_equal = 2 != 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_not_equal = boost::hof::_ != boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_not_equal)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_not_equal)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_not_equal(2, 1) == x_not_equal);

    const auto x_bit_and = 2 & 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_and = boost::hof::_ & boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_and)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_bit_and)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_bit_and(2, 1) == x_bit_and);

    const auto x_xor_ = 2 ^ 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_xor_ = boost::hof::_ ^ boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_xor_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_xor_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_xor_(2, 1) == x_xor_);

    const auto x_bit_or = 2 | 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_or = boost::hof::_ | boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_or)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_bit_or)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_bit_or(2, 1) == x_bit_or);

    const auto x_and_ = true && false;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_and_ = boost::hof::_ && boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_and_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_and_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_and_(true, false) == x_and_);

    const auto x_or_ = true;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_or_ = boost::hof::_ || boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_or_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_or_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_or_(true, false) == x_or_);
}

BOOST_HOF_TEST_CASE()
{
    const auto x_add = 2 + 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_add = 2 + boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_add)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_add)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_add(1) == x_add);

    const auto x_subtract = 2 - 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_subtract = 2 - boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_subtract)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_subtract)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_subtract(1) == x_subtract);

    const auto x_multiply = 2 * 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_multiply = 2 * boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_multiply)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_multiply)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_multiply(1) == x_multiply);

    const auto x_divide = 2 / 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_divide = 2 / boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_divide)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_divide)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_divide(1) == x_divide);

    const auto x_remainder = 2 % 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_remainder = 2 % boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_remainder)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_remainder)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_remainder(1) == x_remainder);

    const auto x_shift_right = 2 >> 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_right = 2 >> boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_right)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_shift_right)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_shift_right(1) == x_shift_right);

    const auto x_shift_left = 2 << 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_left = 2 << boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_left)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_shift_left)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_shift_left(1) == x_shift_left);

    const auto x_greater_than = 2 > 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than = 2 > boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_greater_than)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_greater_than(1) == x_greater_than);

    const auto x_less_than = 2 < 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than = 2 < boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_less_than)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_less_than(1) == x_less_than);

    const auto x_less_than_equal = 2 <= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than_equal = 2 <= boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_less_than_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_less_than_equal(1) == x_less_than_equal);

    const auto x_greater_than_equal = 2 >= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than_equal = 2 >= boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_greater_than_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_greater_than_equal(1) == x_greater_than_equal);

    const auto x_equal = 2 == 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_equal = 2 == boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_equal(1) == x_equal);

    const auto x_not_equal = 2 != 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_not_equal = 2 != boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_not_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_not_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_not_equal(1) == x_not_equal);

    const auto x_bit_and = 2 & 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_and = 2 & boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_and)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_bit_and)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_bit_and(1) == x_bit_and);

    const auto x_xor_ = 2 ^ 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_xor_ = 2 ^ boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_xor_)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_xor_)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_xor_(1) == x_xor_);

    const auto x_bit_or = 2 | 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_or = 2 | boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_or)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_bit_or)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_bit_or(1) == x_bit_or);

    const auto x_and_ = true && false;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_and_ = true && boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_and_)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_and_)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_and_(false) == x_and_);

    const auto x_or_ = true;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_or_ = true || boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_or_)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_or_)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_or_(false) == x_or_);
}

BOOST_HOF_TEST_CASE()
{
    const auto x_add = 2 + 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_add = boost::hof::_ + 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_add)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_add)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_add(2) == x_add);

    const auto x_subtract = 2 - 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_subtract = boost::hof::_ - 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_subtract)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_subtract)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_subtract(2) == x_subtract);

    const auto x_multiply = 2 * 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_multiply = boost::hof::_ * 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_multiply)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_multiply)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_multiply(2) == x_multiply);

    const auto x_divide = 2 / 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_divide = boost::hof::_ / 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_divide)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_divide)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_divide(2) == x_divide);

    const auto x_remainder = 2 % 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_remainder = boost::hof::_ % 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_remainder)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_remainder)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_remainder(2) == x_remainder);

    const auto x_shift_right = 2 >> 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_right = boost::hof::_ >> 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_right)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_shift_right)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_shift_right(2) == x_shift_right);

    const auto x_shift_left = 2 << 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_shift_left = boost::hof::_ << 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_shift_left)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_shift_left)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_shift_left(2) == x_shift_left);

    const auto x_greater_than = 2 > 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than = boost::hof::_ > 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_greater_than)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_greater_than(2) == x_greater_than);

    const auto x_less_than = 2 < 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than = boost::hof::_ < 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_less_than)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_less_than(2) == x_less_than);

    const auto x_less_than_equal = 2 <= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_less_than_equal = boost::hof::_ <= 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_less_than_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_less_than_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_less_than_equal(2) == x_less_than_equal);

    const auto x_greater_than_equal = 2 >= 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_greater_than_equal = boost::hof::_ >= 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_greater_than_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_greater_than_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_greater_than_equal(2) == x_greater_than_equal);

    const auto x_equal = 2 == 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_equal = boost::hof::_ == 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_equal(2) == x_equal);

    const auto x_not_equal = 2 != 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_not_equal = boost::hof::_ != 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_not_equal)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_not_equal)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_not_equal(2) == x_not_equal);

    const auto x_bit_and = 2 & 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_and = boost::hof::_ & 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_and)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_bit_and)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_bit_and(2) == x_bit_and);

    const auto x_xor_ = 2 ^ 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_xor_ = boost::hof::_ ^ 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_xor_)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_xor_)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_xor_(2) == x_xor_);

    const auto x_bit_or = 2 | 1;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_bit_or = boost::hof::_ | 1;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_bit_or)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_bit_or)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_bit_or(2) == x_bit_or);

    const auto x_and_ = true && false;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_and_ = boost::hof::_ && false;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_and_)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_and_)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_and_(true) == x_and_);

    const auto x_or_ = true;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_or_ = boost::hof::_ || false;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_or_)>::value, "Not copyable");
#endif
    static_assert(!boost::hof::detail::is_default_constructible<decltype(f_or_)>::value, "default constructible");
    PLACEHOLDER_CHECK(f_or_(true) == x_or_);
}

BOOST_HOF_TEST_CASE()
{

    const auto x_not_ = !false;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_not_ = !boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_not_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_not_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_not_(false) == x_not_);

    const auto x_compl_ = ~2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_compl_ = ~boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_compl_)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_compl_)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_compl_(2) == x_compl_);

    const auto x_unary_plus = +2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_unary_plus = +boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_unary_plus)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_unary_plus)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_unary_plus(2) == x_unary_plus);

    const auto x_unary_subtract = -2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_unary_subtract = -boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_unary_subtract)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_unary_subtract)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_unary_subtract(2) == x_unary_subtract);

    const auto x_dereference = 2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_dereference = *boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_dereference)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_dereference)>::value, "Not default constructible");
    PLACEHOLDER_CHECK(f_dereference(&x_dereference) == x_dereference);

    // TODO: Test constexpr increment and decrement
#ifndef _MSC_VER
    auto x_increment = 2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_increment = ++boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_increment)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_increment)>::value, "Not default constructible");
    f_increment(x_increment);
    BOOST_HOF_TEST_CHECK(x_increment == 3);

    auto x_decrement = 2;
    BOOST_HOF_PLACEHOLDER_TEST_CONSTEXPR auto f_decrement = --boost::hof::_;
#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ > 6   
    static_assert(std::is_copy_constructible<decltype(f_decrement)>::value, "Not copyable");
#endif
    static_assert(boost::hof::detail::is_default_constructible<decltype(f_decrement)>::value, "Not default constructible");
    f_decrement(x_decrement);
    BOOST_HOF_TEST_CHECK(x_decrement == 1);
#endif

    // TODO: Test post increment and decrement
}

