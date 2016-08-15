// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_SEQ_HPP
#define TEST_SUPPORT_SEQ_HPP

#include <boost/hana/fwd/at.hpp>
#include <boost/hana/fwd/concept/sequence.hpp>
#include <boost/hana/fwd/core/make.hpp>
#include <boost/hana/fwd/drop_front.hpp>
#include <boost/hana/fwd/fold_left.hpp>
#include <boost/hana/fwd/is_empty.hpp>
#include <boost/hana/fwd/length.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/unpack.hpp>


struct Seq;

template <typename Storage>
struct seq_type {
    explicit constexpr seq_type(Storage s) : storage(s) { }
    Storage storage;
    using hana_tag = Seq;
};

struct seq_t {
    template <typename ...Xs>
    constexpr decltype(auto) operator()(Xs ...xs) const {
        auto storage = boost::hana::make_tuple(xs...);
        return seq_type<decltype(storage)>(storage);
    }
};
constexpr seq_t seq{};

namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //
    // Define either one to select which MCD is used:
    //  BOOST_HANA_TEST_FOLDABLE_FOLD_LEFT_MCD
    //  BOOST_HANA_TEST_FOLDABLE_UNPACK_MCD
    //  BOOST_HANA_TEST_FOLDABLE_ITERABLE_MCD
    //
    // If neither is defined, the MCD used is unspecified.
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_FOLDABLE_FOLD_LEFT_MCD
    template <>
    struct fold_left_impl<Seq> {
        template <typename Xs, typename S, typename F>
        static constexpr auto apply(Xs xs, S s, F f) {
            return hana::fold_left(xs.storage, s, f);
        }

        template <typename Xs, typename F>
        static constexpr auto apply(Xs xs, F f) {
            return hana::fold_left(xs.storage, f);
        }
    };
#elif defined(BOOST_HANA_TEST_FOLDABLE_ITERABLE_MCD)
    template <>
    struct length_impl<Seq> {
        template <typename Xs>
        static constexpr auto apply(Xs const& xs) {
            return hana::length(xs.storage);
        }
    };
#else
    template <>
    struct unpack_impl<Seq> {
        template <typename Xs, typename F>
        static constexpr auto apply(Xs xs, F f)
        { return hana::unpack(xs.storage, f); }
    };
#endif

    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct at_impl<Seq> {
        template <typename Xs, typename N>
        static constexpr decltype(auto) apply(Xs&& xs, N&& n) {
            return hana::at(static_cast<Xs&&>(xs).storage, n);
        }
    };

    template <>
    struct drop_front_impl<Seq> {
        template <typename Xs, typename N>
        static constexpr auto apply(Xs xs, N n) {
            return hana::unpack(hana::drop_front(xs.storage, n), ::seq);
        }
    };

    template <>
    struct is_empty_impl<Seq> {
        template <typename Xs>
        static constexpr auto apply(Xs xs) {
            return hana::is_empty(xs.storage);
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // Sequence
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct Sequence<Seq> {
        static constexpr bool value = true;
    };

    template <>
    struct make_impl<Seq> {
        template <typename ...Xs>
        static constexpr auto apply(Xs&& ...xs) {
            return ::seq(static_cast<Xs&&>(xs)...);
        }
    };
}} // end namespace boost::hana

#endif // !TEST_SUPPORT_SEQ_HPP
