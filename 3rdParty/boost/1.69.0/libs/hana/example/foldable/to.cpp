// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    static_assert(hana::to<hana::tuple_tag>(hana::just(1)) == hana::make_tuple(1), "");
    BOOST_HANA_CONSTANT_CHECK(hana::to<hana::tuple_tag>(hana::nothing) == hana::make_tuple());

    BOOST_HANA_CONSTANT_CHECK(
        hana::to<hana::tuple_tag>(hana::make_range(hana::int_c<3>, hana::int_c<6>))
            ==
        hana::tuple_c<int, 3, 4, 5>
    );
}
