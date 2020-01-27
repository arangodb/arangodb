// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/replicate.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::replicate<hana::tuple_tag>('x', hana::size_c<2>) == hana::make_tuple('x', 'x'), "");

// Of course, there can't be more than one element in an `optional`.
static_assert(hana::replicate<hana::optional_tag>('x', hana::size_c<2>) == hana::just('x'), "");

int main() { }
