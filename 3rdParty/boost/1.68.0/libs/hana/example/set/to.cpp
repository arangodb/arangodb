// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto xs = hana::make_tuple(
        hana::int_c<1>,
        hana::int_c<3>,
        hana::type_c<int>,
        hana::long_c<1>
    );

    BOOST_HANA_CONSTANT_CHECK(
        hana::to<hana::set_tag>(xs)
            ==
        hana::make_set(hana::int_c<1>, hana::int_c<3>, hana::type_c<int>)
    );
}
