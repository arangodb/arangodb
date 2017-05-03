// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/is_disjoint.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <string>
namespace hana = boost::hana;
using namespace std::literals;


int main() {
    // Tuples
    auto xs = hana::make_tuple(hana::int_c<1>, "alfa"s, hana::type_c<int>);
    auto ys = hana::make_tuple(hana::type_c<void>, hana::int_c<3>, "bravo"s);
    BOOST_HANA_RUNTIME_CHECK(hana::is_disjoint(xs, ys));

    // Sets
    auto s1 = hana::make_set(hana::int_c<1>, hana::type_c<void>, hana::int_c<2>);
    auto s2 = hana::make_set(hana::type_c<char>, hana::type_c<int>, hana::int_c<1>);
    BOOST_HANA_CONSTANT_CHECK(!hana::is_disjoint(s1, s2));

    // Maps
    auto vowels = hana::make_map(
        hana::make_pair(hana::char_c<'a'>, "alfa"s),
        hana::make_pair(hana::char_c<'e'>, "echo"s),
        hana::make_pair(hana::char_c<'i'>, "india"s)
        // ...
    );

    auto consonants = hana::make_map(
        hana::make_pair(hana::char_c<'b'>, "bravo"s),
        hana::make_pair(hana::char_c<'c'>, "charlie"s),
        hana::make_pair(hana::char_c<'f'>, "foxtrot"s)
        // ...
    );

    BOOST_HANA_CONSTANT_CHECK(hana::is_disjoint(vowels, consonants));
}
