// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/transform.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::transform(hana::nothing, hana::_ + 1) == hana::nothing);
static_assert(hana::transform(hana::just(1), hana::_ + 1) == hana::just(2), "");

int main() { }
