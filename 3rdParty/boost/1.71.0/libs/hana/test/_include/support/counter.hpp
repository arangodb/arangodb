// Copyright Louis Dionne 2013-2017
// Copyright Jason Rice 2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_COUNTER_HPP
#define TEST_SUPPORT_COUNTER_HPP

#include <boost/hana/fwd/at.hpp>
#include <boost/hana/fwd/concept/iterable.hpp>
#include <boost/hana/fwd/drop_front.hpp>
#include <boost/hana/fwd/is_empty.hpp>
#include <boost/hana/integral_constant.hpp>


// Counter - an infinite iterable for the masses

struct Counter_tag { };

template <std::size_t N = 0>
struct Counter {
    using hana_tag = Counter_tag;
    static constexpr std::size_t value = N;
};


namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct at_impl<Counter_tag> {
        template <typename T, typename N>
        static constexpr decltype(auto) apply(T, N) {
            return hana::size_c<T::value + N::value>;
        }
    };

    template <>
    struct drop_front_impl<Counter_tag> {
        template <typename T, typename N>
        static constexpr auto apply(T, N) {
            return Counter<T::value + N::value>{};
        }
    };

    template <>
    struct is_empty_impl<Counter_tag> {
        template <typename Xs>
        static constexpr auto apply(Xs)
            -> hana::false_
        { return {}; }
    };
}} // end namespace boost::hana

#endif // !TEST_SUPPORT_COUNTER_HPP
