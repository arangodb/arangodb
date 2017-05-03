// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/and.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::and_(hana::true_c, hana::true_c, hana::true_c, hana::true_c));
static_assert(!hana::and_(hana::true_c, false, hana::true_c, hana::true_c), "");

int main() { }
