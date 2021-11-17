// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    static_assert(hana::equal(hana::make_tuple(1, 2), hana::make_tuple(1, 2)), "");
    static_assert(!hana::equal('x', 'y'), "");
    BOOST_HANA_CONSTANT_CHECK(!hana::equal(hana::make_tuple(1, 2), 'y'));

    static_assert(hana::any_of(hana::make_tuple(1, 2, 3), hana::equal.to(2)), "");
}
