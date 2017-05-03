// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_NUMERIC_HPP
#define TEST_SUPPORT_NUMERIC_HPP

#include <boost/hana/core/tag_of.hpp>
#include <boost/hana/eval.hpp>
#include <boost/hana/fwd/div.hpp>
#include <boost/hana/fwd/equal.hpp>
#include <boost/hana/fwd/eval_if.hpp>
#include <boost/hana/fwd/less.hpp>
#include <boost/hana/fwd/minus.hpp>
#include <boost/hana/fwd/mod.hpp>
#include <boost/hana/fwd/mult.hpp>
#include <boost/hana/fwd/negate.hpp>
#include <boost/hana/fwd/not.hpp>
#include <boost/hana/fwd/one.hpp>
#include <boost/hana/fwd/plus.hpp>
#include <boost/hana/fwd/while.hpp>
#include <boost/hana/fwd/zero.hpp>


struct numeric_type {
    constexpr explicit numeric_type(int v) : value(v) { }
    int value;
    constexpr operator int() const { return value; }
};

using Numeric = boost::hana::tag_of_t<numeric_type>;

struct numeric_t {
    constexpr numeric_type operator()(int x) const {
        return numeric_type{x};
    }
};
constexpr numeric_t numeric{};


namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct equal_impl<Numeric, Numeric> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return numeric(x.value == y.value); }
    };

    //////////////////////////////////////////////////////////////////////////
    // Orderable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct less_impl<Numeric, Numeric> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y) {
            // Workaround a _weird_ GCC bug:
            // error: parse error in template argument list
            //      bool cmp = (x.value < y.value);
            //                    ^
            int xv = x.value, yv = y.value;
            return numeric(xv < yv);
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Logical
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct eval_if_impl<Numeric> {
        template <typename C, typename T, typename E>
        static constexpr auto apply(C const& c, T&& t, E&& e) {
            return c.value ? hana::eval(static_cast<T&&>(t))
                           : hana::eval(static_cast<E&&>(e));
        }
    };

    template <>
    struct not_impl<Numeric> {
        template <typename X>
        static constexpr auto apply(X x)
        { return numeric(!x.value); }
    };

    template <>
    struct while_impl<Numeric> {
        template <typename Pred, typename State, typename F>
        static constexpr auto apply(Pred pred, State state, F f)
            -> decltype(true ? f(state) : state)
        {
            if (pred(state))
                return hana::while_(pred, f(state), f);
            else
                return state;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Monoid
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct plus_impl<Numeric, Numeric> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return numeric(x.value + y.value); }
    };

    template <>
    struct zero_impl<Numeric> {
        static constexpr auto apply()
        { return numeric(0); }
    };

    //////////////////////////////////////////////////////////////////////////
    // Group
    //
    // Define either one to select which MCD is used:
    //  BOOST_HANA_TEST_GROUP_NEGATE_MCD
    //  BOOST_HANA_TEST_GROUP_MINUS_MCD
    //
    // If neither is defined, the MCD used is unspecified.
    //////////////////////////////////////////////////////////////////////////
#if defined(BOOST_HANA_TEST_GROUP_NEGATE_MCD)
    template <>
    struct negate_impl<Numeric> {
        template <typename X>
        static constexpr auto apply(X x)
        { return numeric(-x.value); }
    };
#else
    template <>
    struct minus_impl<Numeric, Numeric> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return numeric(x.value - y.value); }
    };
#endif

    //////////////////////////////////////////////////////////////////////////
    // Ring
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct mult_impl<Numeric, Numeric> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return numeric(x.value * y.value); }
    };

    template <>
    struct one_impl<Numeric> {
        static constexpr auto apply()
        { return numeric(1); }
    };

    //////////////////////////////////////////////////////////////////////////
    // EuclideanRing
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct div_impl<Numeric, Numeric> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return numeric(x.value / y.value); }
    };

    template <>
    struct mod_impl<Numeric, Numeric> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return numeric(x.value % y.value); }
    };
}} // end namespace boost::hana

#endif //! TEST_SUPPORT_NUMERIC_HPP
