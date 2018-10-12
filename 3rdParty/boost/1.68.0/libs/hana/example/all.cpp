// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/all.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::all(hana::make_tuple(hana::true_c, true, hana::true_c)), "");
BOOST_HANA_CONSTANT_CHECK(!hana::all(hana::make_tuple(true, hana::false_c, hana::true_c)));

int main() { }
