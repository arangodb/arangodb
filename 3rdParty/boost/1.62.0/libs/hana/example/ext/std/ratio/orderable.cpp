// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/ratio.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/less_equal.hpp>

#include <ratio>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::less(std::ratio<3, 12>{}, std::ratio<5, 12>{}));
BOOST_HANA_CONSTANT_CHECK(hana::less_equal(std::ratio<6, 12>{}, std::ratio<4, 8>{}));

int main() { }
