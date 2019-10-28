// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


template <typename X, typename Y, typename Z>
struct Triple {
    X first;
    Y second;
    Z third;
};

BOOST_HANA_CONSTEXPR_LAMBDA auto triple = [](auto x, auto y, auto z) {
    return Triple<decltype(x), decltype(y), decltype(z)>{x, y, z};
};

namespace boost { namespace hana {
    template <typename X, typename Y, typename Z>
    struct to_impl<tuple_tag, Triple<X, Y, Z>> {
        static constexpr auto apply(Triple<X, Y, Z> xs) {
            return make_tuple(xs.first, xs.second, xs.third);
        }
    };
}}

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(
        hana::to<hana::tuple_tag>(triple(1, '2', 3.3)) == hana::make_tuple(1, '2', 3.3)
    );
}
