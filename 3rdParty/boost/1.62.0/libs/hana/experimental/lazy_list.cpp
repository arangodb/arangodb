// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/eval.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/functional/fix.hpp>
#include <boost/hana/functional/iterate.hpp>
#include <boost/hana/fwd/at.hpp>
#include <boost/hana/fwd/empty.hpp>
#include <boost/hana/fwd/prepend.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/value.hpp>

#include <cstddef>
namespace hana = boost::hana;


struct LazyList;

template <typename X, typename Xs>
struct lazy_cons_type {
    X x;
    Xs xs;
    using hana_tag = LazyList;
};

auto lazy_cons = [](auto x, auto xs) {
    return lazy_cons_type<decltype(x), decltype(xs)>{x, xs};
};

struct lazy_nil_type { using hana_tag = LazyList; };

constexpr lazy_nil_type lazy_nil{};

auto repeat = hana::fix([](auto repeat, auto x) {
    return lazy_cons(x, hana::make_lazy(repeat)(x));
});

namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct at_impl<LazyList> {
        template <typename Xs, typename N>
        static constexpr auto apply(Xs&& lcons, N const& n) {
            return hana::drop_front(lcons, n).x;
        }
    };

    namespace detail {
        struct eval_tail {
            template <typename Xs>
            constexpr auto operator()(Xs const& lcons) const {
                return hana::eval(lcons.xs);
            }

            constexpr auto operator()(lazy_nil_type const&) const {
                return lazy_nil;
            }
        };
    }

    template <>
    struct drop_front_impl<LazyList> {
        template <typename Xs, typename N>
        static constexpr auto apply(Xs&& lcons, N const&) {
            return hana::iterate<N::value>(detail::eval_tail{}, lcons);
        }
    };

    template <>
    struct is_empty_impl<LazyList> {
        template <typename Xs>
        static constexpr auto apply(Xs const&)
        { return hana::false_c; }

        static constexpr auto apply(lazy_nil_type const&)
        { return hana::true_c; }
    };

    //////////////////////////////////////////////////////////////////////////
    // MonadPlus
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct prepend_impl<LazyList> {
        template <typename Xs, typename X>
        static constexpr auto apply(Xs xs, X x)
        { return lazy_cons(x, hana::make_lazy(xs)); }
    };

    template <>
    struct empty_impl<LazyList> {
        static constexpr auto apply()
        { return lazy_nil; }
    };
}}


int main() {
    BOOST_HANA_CONSTANT_CHECK(!hana::is_empty(repeat(1)));
    BOOST_HANA_CONSTEXPR_CHECK(hana::front(repeat(1)) == 1);
    BOOST_HANA_CONSTEXPR_CHECK(hana::at(repeat(1), hana::size_c<10>) == 1);
}
