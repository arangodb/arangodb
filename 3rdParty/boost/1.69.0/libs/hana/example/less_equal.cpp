// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less_equal.hpp>
#include <boost/hana/not.hpp>
namespace hana = boost::hana;


static_assert(hana::less_equal(1, 4), "");
static_assert(hana::less_equal(1, 1), "");
BOOST_HANA_CONSTANT_CHECK(!hana::less_equal(hana::int_c<3>, hana::int_c<2>));

int main() { }
