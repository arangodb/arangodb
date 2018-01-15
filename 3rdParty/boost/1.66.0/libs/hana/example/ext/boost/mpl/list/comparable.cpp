// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/boost/mpl/list.hpp>
#include <boost/hana/not_equal.hpp>

#include <boost/mpl/list.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


BOOST_HANA_CONSTANT_CHECK(
    hana::equal(mpl::list2<int, char>{}, mpl::list<int, char>{})
);
BOOST_HANA_CONSTANT_CHECK(
    hana::not_equal(mpl::list2<int, char>{}, mpl::list<int, char, float>{})
);

int main() { }
