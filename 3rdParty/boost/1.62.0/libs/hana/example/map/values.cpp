// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/permutations.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <string>
namespace hana = boost::hana;
using namespace std::literals;


int main() {
    auto m = hana::make_map(
        hana::make_pair(hana::int_c<1>, "foobar"s),
        hana::make_pair(hana::type_c<void>, 1234)
    );

    // The order of the values is unspecified.
    BOOST_HANA_RUNTIME_CHECK(
        hana::values(m) ^hana::in^ hana::permutations(hana::make_tuple("foobar"s, 1234))
    );
}
