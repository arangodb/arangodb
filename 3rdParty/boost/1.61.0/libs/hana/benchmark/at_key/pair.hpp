// Copyright Louis Dionne 2013-2016
// Copyright Jason Rice 2015
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BENCHMARK_AT_KEY_PAIR_HPP
#define BENCHMARK_AT_KEY_PAIR_HPP

#include <boost/hana/fwd/first.hpp>
#include <boost/hana/fwd/second.hpp>


struct light_pair_tag { };

template <typename T, typename U>
struct light_pair {
    using First = T;
    using Second = U;
    using hana_tag = light_pair_tag;
};

namespace boost { namespace hana {
    template <>
    struct first_impl<light_pair_tag> {
        template <typename First, typename Second>
        static constexpr First apply(light_pair<First, Second> const&)
        { return First{}; }
    };

    template<>
    struct second_impl<light_pair_tag> {
        template <typename First, typename Second>
        static constexpr Second apply(light_pair<First, Second> const&)
        { return Second{}; }
    };
}} // end namespace boost::hana

#endif // !BENCHMARK_AT_KEY_PAIR_HPP
