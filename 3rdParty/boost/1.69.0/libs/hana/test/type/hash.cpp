// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


struct T;

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::hash(hana::type_c<T>),
    hana::type_c<T>
));

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::hash(hana::basic_type<T>{}),
    hana::type_c<T>
));

int main() { }
