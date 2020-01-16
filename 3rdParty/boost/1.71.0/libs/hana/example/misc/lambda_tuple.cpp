// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/detail/variadic/at.hpp>
#include <boost/hana/detail/variadic/drop_into.hpp>
#include <boost/hana/detail/variadic/take.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/functional/id.hpp>
#include <boost/hana/functional/on.hpp>
#include <boost/hana/fwd/append.hpp>
#include <boost/hana/fwd/at.hpp>
#include <boost/hana/fwd/concat.hpp>
#include <boost/hana/fwd/concept/sequence.hpp>
#include <boost/hana/fwd/core/make.hpp>
#include <boost/hana/fwd/drop_front.hpp>
#include <boost/hana/fwd/empty.hpp>
#include <boost/hana/fwd/front.hpp>
#include <boost/hana/fwd/prepend.hpp>
#include <boost/hana/fwd/take_front.hpp>
#include <boost/hana/fwd/transform.hpp>
#include <boost/hana/fwd/unpack.hpp>
#include <boost/hana/fwd/zip_shortest_with.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/min.hpp>
#include <boost/hana/minimum.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/unpack.hpp>

#include <utility>
namespace hana = boost::hana;


// An interesting way of implementing tuple using lambda captures.

struct lambda_tuple_tag { };

template <typename Storage>
struct lambda_tuple_t {
    explicit constexpr lambda_tuple_t(Storage&& s)
        : storage(std::move(s))
    { }

    using hana_tag = lambda_tuple_tag;
    Storage storage;
};

auto lambda_tuple = [](auto ...xs) {
    auto storage = [=](auto f) -> decltype(auto) { return f(xs...); };
    return lambda_tuple_t<decltype(storage)>{std::move(storage)};
};

namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct unpack_impl<lambda_tuple_tag> {
        template <typename Xs, typename F>
        static constexpr decltype(auto) apply(Xs&& xs, F&& f) {
            return static_cast<Xs&&>(xs).storage(static_cast<F&&>(f));
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Functor
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct transform_impl<lambda_tuple_tag> {
        template <typename Xs, typename F>
        static constexpr decltype(auto) apply(Xs&& xs, F f) {
            return static_cast<Xs&&>(xs).storage(
                [f(std::move(f))](auto&& ...xs) -> decltype(auto) {
                    return lambda_tuple(f(static_cast<decltype(xs)&&>(xs))...);
                }
            );
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct front_impl<lambda_tuple_tag> {
        template <typename Xs>
        static constexpr decltype(auto) apply(Xs&& xs) {
            return static_cast<Xs&&>(xs).storage(
                [](auto&& x, auto&& ...) -> decltype(auto) {
                    return id(static_cast<decltype(x)&&>(x));
                }
            );
        }
    };

    template <>
    struct is_empty_impl<lambda_tuple_tag> {
        template <typename Xs>
        static constexpr decltype(auto) apply(Xs&& xs) {
            return static_cast<Xs&&>(xs).storage(
                [](auto const& ...xs) -> decltype(auto) {
                    return hana::bool_c<sizeof...(xs) == 0>;
                }
            );
        }
    };

    template <>
    struct at_impl<lambda_tuple_tag> {
        template <typename Xs, typename Index>
        static constexpr decltype(auto) apply(Xs&& xs, Index const&) {
            return static_cast<Xs&&>(xs).storage(
                detail::variadic::at<Index::value>
            );
        }
    };

    template <>
    struct drop_front_impl<lambda_tuple_tag> {
        template <typename Xs, typename N>
        static constexpr decltype(auto) apply(Xs&& xs, N const& n) {
            auto m = min(n, length(xs));
            return static_cast<Xs&&>(xs).storage(
                detail::variadic::drop_into<hana::value(m)>(lambda_tuple)
            );
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // MonadPlus
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct concat_impl<lambda_tuple_tag> {
        template <typename Xs, typename Ys>
        static constexpr decltype(auto) apply(Xs&& xs, Ys&& ys) {
            return static_cast<Xs&&>(xs).storage(
                [ys(static_cast<Ys&&>(ys))](auto&& ...xs) -> decltype(auto) {
                    return std::move(ys).storage(
                        // We can't initialize the capture with perfect
                        // forwarding since that's not supported by the
                        // language.
                        [=](auto&& ...ys) -> decltype(auto) {
                            return lambda_tuple(
                                std::move(xs)...,
                                static_cast<decltype(ys)&&>(ys)...
                            );
                        }
                    );
                }
            );
        }
    };

    template <>
    struct prepend_impl<lambda_tuple_tag> {
        template <typename Xs, typename X>
        static constexpr decltype(auto) apply(Xs&& xs, X&& x) {
            return static_cast<Xs&&>(xs).storage(
                [x(static_cast<X&&>(x))](auto&& ...xs) -> decltype(auto) {
                    return lambda_tuple(
                        std::move(x),
                        static_cast<decltype(xs)&&>(xs)...
                    );
                }
            );
        }
    };

    template <>
    struct append_impl<lambda_tuple_tag> {
        template <typename Xs, typename X>
        static constexpr decltype(auto) apply(Xs&& xs, X&& x) {
            return static_cast<Xs&&>(xs).storage(
                [x(static_cast<X&&>(x))](auto&& ...xs) -> decltype(auto) {
                    return lambda_tuple(
                        static_cast<decltype(xs)&&>(xs)...,
                        std::move(x)
                    );
                }
            );
        }
    };

    template <>
    struct empty_impl<lambda_tuple_tag> {
        static BOOST_HANA_CONSTEXPR_LAMBDA decltype(auto) apply() {
            return lambda_tuple();
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Sequence
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct Sequence<lambda_tuple_tag> {
        static constexpr bool value = true;
    };

    template <>
    struct take_front_impl<lambda_tuple_tag> {
        template <typename Xs, typename N>
        static constexpr decltype(auto) apply(Xs&& xs, N const& n) {
            auto m = min(n, length(xs));
            return static_cast<Xs&&>(xs).storage(
                detail::variadic::take<decltype(m)::value>
            )(lambda_tuple);
        }
    };

    template <>
    struct zip_shortest_with_impl<lambda_tuple_tag> {
        template <typename F, typename ...Xss>
        static constexpr auto apply(F f, Xss ...tuples) {
            auto go = [=](auto index, auto ...nothing) {
                return always(f)(nothing...)(at(tuples, index)...);
            };
            auto zip_length = minimum(lambda_tuple(length(tuples)...));
            return unpack(make_range(size_c<0>, zip_length),
                on(lambda_tuple, go)
            );
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // make
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct make_impl<lambda_tuple_tag> {
        template <typename ...Xs>
        static constexpr decltype(auto) apply(Xs&& ...xs) {
            return lambda_tuple(static_cast<Xs&&>(xs)...);
        }
    };
}} // end namespace boost::hana


int main() {
    auto xs = lambda_tuple(1, '2', 3.3);
    static_assert(!decltype(hana::is_empty(xs))::value, "");
}
