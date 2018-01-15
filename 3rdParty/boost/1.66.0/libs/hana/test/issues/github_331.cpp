// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>
#include <boost/hana/tuple.hpp>

#include <type_traits>
#include <utility>
namespace hana = boost::hana;


// In GitHub issue #331, we noticed that `first` and `second` could sometimes
// return the wrong member in case of nested pairs. This is due to the way we
// inherit from base classes to enable EBO. We also check for `basic_tuple`,
// because both are implemented similarly.

int main() {
    {
        using Nested = hana::pair<hana::int_<1>, hana::int_<2>>;
        using Pair = hana::pair<hana::int_<0>, Nested>;
        Pair pair{};

        auto a = hana::first(pair);
        static_assert(std::is_same<decltype(a), hana::int_<0>>{}, "");

        auto b = hana::second(pair);
        static_assert(std::is_same<decltype(b), Nested>{}, "");
    }

    {
        using Nested = hana::basic_tuple<hana::int_<1>, hana::int_<2>>;
        using Tuple = hana::basic_tuple<hana::int_<0>, Nested>;
        Tuple tuple{};

        auto a = hana::at_c<0>(tuple);
        static_assert(std::is_same<decltype(a), hana::int_<0>>{}, "");

        auto b = hana::at_c<1>(tuple);
        static_assert(std::is_same<decltype(b), Nested>{}, "");
    }

    // Original test case submitted by Vittorio Romeo
    {
        hana::pair<hana::int_<1>, hana::bool_<false>> p{};
        auto copy = hana::make_pair(hana::int_c<0>, p);
        auto move = hana::make_pair(hana::int_c<0>, std::move(p));

        copy = move; // copy assign
        copy = std::move(move); // move assign
    }
}
