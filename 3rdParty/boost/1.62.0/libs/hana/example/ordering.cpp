// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ordering.hpp>
#include <boost/hana/sort.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


constexpr auto sorted = hana::sort.by(hana::ordering(hana::sizeof_),
    hana::tuple_t<char[3], char[1], char[2], char[15]>
);

BOOST_HANA_CONSTANT_CHECK(sorted == hana::tuple_t<char[1], char[2], char[3], char[15]>);

int main() { }
