// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/append.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::append(hana::make_tuple(), 1) == hana::make_tuple(1), "");
static_assert(hana::append(hana::make_tuple(1, '2'), 3.3) == hana::make_tuple(1, '2', 3.3), "");
static_assert(hana::append(hana::append(hana::append(hana::make_tuple(), 1), '2'), 3.3) == hana::make_tuple(1, '2', 3.3), "");

int main() { }
