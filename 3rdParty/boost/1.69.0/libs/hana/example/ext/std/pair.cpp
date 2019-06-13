// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/second.hpp>
namespace hana = boost::hana;


constexpr auto pair = std::make_pair(1, 'x');

static_assert(hana::first(pair) == 1, "");
static_assert(hana::second(pair) == 'x', "");

static_assert(hana::not_equal(pair, std::make_pair(3, 'z')), "");
static_assert(hana::less(pair, std::make_pair(3, 'x')), "");

int main() { }
