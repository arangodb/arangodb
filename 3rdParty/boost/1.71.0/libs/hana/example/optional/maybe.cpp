// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


static_assert(hana::maybe('x', hana::_ + 1, hana::just(1)) == 2, "");
static_assert(hana::maybe('x', hana::_ + 1, hana::nothing) == 'x', "");

int main() { }
