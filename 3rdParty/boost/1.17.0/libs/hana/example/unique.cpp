// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/comparing.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/unique.hpp>

#include <string>
namespace hana = boost::hana;
using namespace std::literals;


// unique without a predicate
constexpr auto types = hana::tuple_t<int, float, float, char, int, int, int, double>;
BOOST_HANA_CONSTANT_CHECK(
    hana::unique(types) == hana::tuple_t<int, float, char, int, double>
);

int main() {
    // unique with a predicate
    auto objects = hana::make_tuple(1, 2, "abc"s, 'd', "efg"s, "hij"s, 3.4f);
    BOOST_HANA_RUNTIME_CHECK(
        hana::unique(objects, [](auto const& t, auto const& u) {
            return hana::typeid_(t) == hana::typeid_(u);
        })
        == hana::make_tuple(1, "abc"s, 'd', "efg"s, 3.4f)
    );

    // unique.by is syntactic sugar
    BOOST_HANA_RUNTIME_CHECK(
        hana::unique.by(hana::comparing(hana::typeid_), objects) ==
            hana::make_tuple(1, "abc"s, 'd', "efg"s, 3.4f)
    );
}
