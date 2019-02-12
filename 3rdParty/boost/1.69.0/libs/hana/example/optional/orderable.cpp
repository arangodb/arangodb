// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::nothing < hana::just(3));
BOOST_HANA_CONSTANT_CHECK(hana::just(0) > hana::nothing);
static_assert(hana::just(1) < hana::just(3), "");
static_assert(hana::just(3) > hana::just(2), "");

int main() { }
