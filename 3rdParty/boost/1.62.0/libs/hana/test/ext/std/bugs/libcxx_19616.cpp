// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/lift.hpp>

#include <tuple>
using namespace boost::hana;


int main() {
    // Make sure we workaround the bug at:
    // http://llvm.org/bugs/show_bug.cgi?id=19616
    BOOST_HANA_CONSTEXPR_CHECK(equal(
        lift<ext::std::tuple_tag>(std::make_tuple(1)),
        std::make_tuple(std::make_tuple(1))
    ));
}
