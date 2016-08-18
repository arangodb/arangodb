// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>

#include <type_traits>
#include <utility>
namespace hana = boost::hana;


constexpr std::integer_sequence<int, 0, 1, 2, 3, 4> indices{};

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::at_c<2>(indices),
    std::integral_constant<int, 2>{}
));

int main() { }
