// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_IDENTITY_HPP
#define TEST_SUPPORT_IDENTITY_HPP

#include <boost/hana/chain.hpp>
#include <boost/hana/eval_if.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/functional/partial.hpp>
#include <boost/hana/fwd/adjust_if.hpp>
#include <boost/hana/fwd/ap.hpp>
#include <boost/hana/fwd/equal.hpp>
#include <boost/hana/fwd/flatten.hpp>
#include <boost/hana/fwd/less.hpp>
#include <boost/hana/fwd/lift.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/transform.hpp>

#include <type_traits>


struct Identity;

template <typename T>
struct identity_t {
    T value;
    using hana_tag = Identity;
};

struct make_identity {
    template <typename T>
    constexpr identity_t<typename std::decay<T>::type> operator()(T&& t) const {
        return {static_cast<T&&>(t)};
    }
};

constexpr make_identity identity{};


namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct equal_impl<Identity, Identity> {
        template <typename Id1, typename Id2>
        static constexpr auto apply(Id1 x, Id2 y)
        { return hana::equal(x.value, y.value); }
    };

    //////////////////////////////////////////////////////////////////////////
    // Orderable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct less_impl<Identity, Identity> {
        template <typename Id1, typename Id2>
        static constexpr auto apply(Id1 x, Id2 y)
        { return hana::less(x.value, y.value); }
    };

    //////////////////////////////////////////////////////////////////////////
    // Functor
    //
    // Define either one to select which MCD is used:
    //  BOOST_HANA_TEST_FUNCTOR_TRANSFORM_MCD
    //  BOOST_HANA_TEST_FUNCTOR_ADJUST_MCD_MCD
    //
    // If neither is defined, the MCD used is unspecified.
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_FUNCTOR_TRANSFORM_MCD
    template <>
    struct transform_impl<Identity> {
        template <typename Id, typename F>
        static constexpr auto apply(Id self, F f)
        { return ::identity(f(self.value)); }
    };
#else
    template <>
    struct adjust_if_impl<Identity> {
        struct get_value {
            template <typename T>
            constexpr auto operator()(T t) const { return t.value; }
        };

        template <typename Id, typename P, typename F>
        static constexpr auto apply(Id self, P p, F f) {
            auto x = hana::eval_if(p(self.value),
                hana::make_lazy(hana::compose(f, get_value{}))(self),
                hana::make_lazy(get_value{})(self)
            );
            return ::identity(x);
        }
    };
#endif

    //////////////////////////////////////////////////////////////////////////
    // Applicative
    //
    // Define either one to select which MCD is used:
    //  BOOST_HANA_TEST_APPLICATIVE_FULL_MCD
    //  BOOST_HANA_TEST_APPLICATIVE_MONAD_MCD
    //
    // If neither is defined, the MCD used is unspecified.
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct lift_impl<Identity> {
        template <typename X>
        static constexpr auto apply(X x)
        { return ::identity(x); }
    };
#ifdef BOOST_HANA_TEST_APPLICATIVE_FULL_MCD
    template <>
    struct ap_impl<Identity> {
        template <typename F, typename X>
        static constexpr auto apply(F f, X x)
        { return ::identity(f.value(x.value)); }
    };
#else
    template <>
    struct ap_impl<Identity> {
        template <typename F, typename X>
        static constexpr decltype(auto) apply(F&& f, X&& x) {
            return hana::chain(
                static_cast<F&&>(f),
                hana::partial(hana::transform, static_cast<X&&>(x))
            );
        }
    };
#endif

    //////////////////////////////////////////////////////////////////////////
    // Monad
    //
    // Define either one to select which MCD is used:
    //  BOOST_HANA_TEST_MONAD_FLATTEN_MCD
    //  BOOST_HANA_TEST_MONAD_CHAIN_MCD
    //
    // If neither is defined, the MCD used is unspecified.
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_MONAD_FLATTEN_MCD
    template <>
    struct flatten_impl<Identity> {
        template <typename Id>
        static constexpr auto apply(Id self)
        { return self.value; }
    };
#else
    template <>
    struct chain_impl<Identity> {
        template <typename X, typename F>
        static constexpr auto apply(X x, F f)
        { return f(x.value); }
    };
#endif
}} // end namespace boost::hana

#endif // !TEST_SUPPORT_IDENTITY_HPP
