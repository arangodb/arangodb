// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::any(hana::make_tuple(false, hana::false_c, hana::true_c)));
static_assert(hana::any(hana::make_tuple(false, hana::false_c, true)), "");
static_assert(!hana::any(hana::make_tuple(false, hana::false_c, hana::false_c)), "");

int main() { }
