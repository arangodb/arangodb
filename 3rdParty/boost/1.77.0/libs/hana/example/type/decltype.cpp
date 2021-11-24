// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


struct X { };
BOOST_HANA_CONSTANT_CHECK(hana::decltype_(X{}) == hana::type_c<X>);
BOOST_HANA_CONSTANT_CHECK(hana::decltype_(hana::type_c<X>) == hana::type_c<X>);

BOOST_HANA_CONSTANT_CHECK(hana::decltype_(1) == hana::type_c<int>);

static int const& i = 1;
BOOST_HANA_CONSTANT_CHECK(hana::decltype_(i) == hana::type_c<int const>);

int main() { }
