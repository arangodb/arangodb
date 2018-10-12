// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/cartesian_product.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


constexpr auto tuples = hana::make_tuple(
    hana::make_tuple(1, 2, 3),
    hana::make_tuple('a', 'b'),
    hana::make_tuple(hana::type_c<int>, hana::type_c<char>)
);

constexpr auto prod = hana::make_tuple(
    hana::make_tuple(1, 'a', hana::type_c<int>),
    hana::make_tuple(1, 'a', hana::type_c<char>),
    hana::make_tuple(1, 'b', hana::type_c<int>),
    hana::make_tuple(1, 'b', hana::type_c<char>),

    hana::make_tuple(2, 'a', hana::type_c<int>),
    hana::make_tuple(2, 'a', hana::type_c<char>),
    hana::make_tuple(2, 'b', hana::type_c<int>),
    hana::make_tuple(2, 'b', hana::type_c<char>),

    hana::make_tuple(3, 'a', hana::type_c<int>),
    hana::make_tuple(3, 'a', hana::type_c<char>),
    hana::make_tuple(3, 'b', hana::type_c<int>),
    hana::make_tuple(3, 'b', hana::type_c<char>)
);

static_assert(hana::cartesian_product(tuples) == prod, "");

int main() { }
