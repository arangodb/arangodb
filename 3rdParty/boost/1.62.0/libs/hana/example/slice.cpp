// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/filter.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/slice.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;
using namespace hana::literals;


// Slice a contiguous range
constexpr auto xs = hana::make_tuple(0, '1', 2.2, 3_c, hana::type_c<float>);

static_assert(
    hana::slice(xs, hana::tuple_c<std::size_t, 1, 2, 3>) ==
    hana::make_tuple('1', 2.2, 3_c)
, "");


// A more complex example with a non-contiguous range
constexpr auto letters = hana::to_tuple(hana::range_c<char, 'a', 'z'>);
constexpr auto indices = hana::to_tuple(hana::make_range(hana::size_c<0>, hana::length(letters)));

auto even_indices = hana::filter(indices, [](auto n) {
    return n % hana::size_c<2> == hana::size_c<0>;
});

BOOST_HANA_CONSTANT_CHECK(
    hana::slice(letters, even_indices) == hana::tuple_c<char,
        'a', 'c', 'e', 'g', 'i', 'k', 'm', 'o', 'q', 's', 'u', 'w', 'y'
    >
);

int main() { }
