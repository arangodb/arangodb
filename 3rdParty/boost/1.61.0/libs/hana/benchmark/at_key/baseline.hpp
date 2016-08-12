// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BENCHMARK_AT_KEY_BASELINE_HPP
#define BENCHMARK_AT_KEY_BASELINE_HPP

namespace baseline {
    template <typename ...T>
    struct map { };

    template <typename ...T>
    map<T...> make_map(T const& ...) {
        return {};
    }

    template <typename Map, typename Key>
    void at_key(Map const&, Key const&) { }
}

#endif // !BENCHMARK_AT_KEY_BASELINE_HPP
