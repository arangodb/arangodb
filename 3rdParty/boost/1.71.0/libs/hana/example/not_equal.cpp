// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/all_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    static_assert(hana::not_equal(hana::make_tuple(1, 2), hana::make_tuple(3)), "");
    static_assert(hana::not_equal('x', 'y'), "");
    BOOST_HANA_CONSTANT_CHECK(hana::not_equal(hana::make_tuple(1, 2), 'y'));

    static_assert(hana::all_of(hana::make_tuple(1, 2, 3), hana::not_equal.to(5)), "");
}
