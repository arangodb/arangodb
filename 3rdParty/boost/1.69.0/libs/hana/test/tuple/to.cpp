// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    // make sure to_tuple == to<tuple_tag>
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::to<hana::tuple_tag>(hana::tuple_t<int, char, void, int(float)>),
        hana::to_tuple(hana::tuple_t<int, char, void, int(float)>)
    ));
}
