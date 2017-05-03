// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/boost/mpl/integral_c.hpp>
#include <boost/hana/not_equal.hpp>

#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c.hpp>
#include <boost/mpl/long.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


static_assert(hana::value(mpl::integral_c<int, 3>{}) == 3, "");
static_assert(mpl::integral_c<int, 3>::value == 3, "");

BOOST_HANA_CONSTANT_CHECK(hana::equal(mpl::integral_c<int, 3>{}, mpl::int_<3>{}));
BOOST_HANA_CONSTANT_CHECK(hana::equal(mpl::integral_c<int, 3>{}, mpl::long_<3>{}));
BOOST_HANA_CONSTANT_CHECK(hana::not_equal(mpl::integral_c<int, 3>{}, mpl::int_<0>{}));

int main() { }
