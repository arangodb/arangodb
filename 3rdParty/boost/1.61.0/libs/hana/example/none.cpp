// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/none.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::none(hana::make_tuple(false, hana::false_c, hana::false_c)), "");
static_assert(!hana::none(hana::make_tuple(false, hana::false_c, true)), "");
BOOST_HANA_CONSTANT_CHECK(!hana::none(hana::make_tuple(false, hana::false_c, hana::true_c)));

int main() { }
