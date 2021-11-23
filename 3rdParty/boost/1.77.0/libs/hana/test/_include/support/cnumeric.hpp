// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_CNUMERIC_HPP
#define TEST_SUPPORT_CNUMERIC_HPP

#include <boost/hana/concept/integral_constant.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/fwd/core/to.hpp>
#include <boost/hana/fwd/equal.hpp>
#include <boost/hana/fwd/less.hpp>


template <typename T>
struct CNumeric { using value_type = T; };

template <typename T, T v>
struct cnumeric_t {
    static constexpr T value = v;
    using hana_tag = CNumeric<T>;
    constexpr operator T() const { return value; }
};

template <typename T, T v>
constexpr cnumeric_t<T, v> cnumeric{};

template <typename T, T v>
constexpr cnumeric_t<T, v> make_cnumeric() { return {}; }

namespace boost { namespace hana {
    // Constant and IntegralConstant
    template <typename T>
    struct IntegralConstant<CNumeric<T>> {
        static constexpr bool value = true;
    };

    template <typename T, typename C>
    struct to_impl<CNumeric<T>, C, when<
        hana::IntegralConstant<C>::value
    >>
        : embedding<is_embedded<typename C::value_type, T>::value>
    {
        template <typename N>
        static constexpr auto apply(N const&)
        { return cnumeric<T, N::value>; }
    };

    // Comparable
    template <typename T, typename U>
    struct equal_impl<CNumeric<T>, CNumeric<U>> {
        template <typename X, typename Y>
        static constexpr auto apply(X const&, Y const&)
        { return cnumeric<bool, X::value == Y::value>; }
    };

    // Orderable
    template <typename T, typename U>
    struct less_impl<CNumeric<T>, CNumeric<U>> {
        template <typename X, typename Y>
        static constexpr auto apply(X const&, Y const&)
        { return cnumeric<bool, (X::value < Y::value)>; }
    };
}} // end namespace boost::hana

#endif // !TEST_SUPPORT_CNUMERIC_HPP
