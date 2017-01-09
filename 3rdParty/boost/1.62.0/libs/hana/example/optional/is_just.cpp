// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK( hana::is_just(hana::just('x')));
BOOST_HANA_CONSTANT_CHECK( hana::is_just(hana::just(hana::nothing)));
BOOST_HANA_CONSTANT_CHECK(!hana::is_just(hana::nothing));

int main() { }
