// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ap.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>

#include <functional>
namespace hana = boost::hana;


int main() {
    // with tuples
    static_assert(
        hana::ap(hana::make_tuple(std::plus<>{}), hana::make_tuple(1, 2),
                                                  hana::make_tuple(3, 4, 5))
            ==
        hana::make_tuple(
            1 + 3,      1 + 4,      1 + 5,
            2 + 3,      2 + 4,      2 + 5
        )
    , "");

    // with optional values
    BOOST_HANA_CONSTEXPR_LAMBDA auto multiply = [](auto a, auto b, auto c) {
        return a * b * c;
    };

    BOOST_HANA_CONSTEXPR_CHECK(
        hana::ap(hana::just(multiply), hana::just(1),
                                       hana::just(2),
                                       hana::just(3))
            ==
        hana::just(1 * 2 * 3)
    );

    BOOST_HANA_CONSTANT_CHECK(
        hana::ap(hana::just(multiply), hana::just(1),
                                       hana::nothing,
                                       hana::just(3))
            ==
        hana::nothing
    );
}
