// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(
        hana::to_tuple(hana::tuple_c<int, 0, 1, 2>)
            ==
        hana::make_tuple(hana::int_c<0>, hana::int_c<1>, hana::int_c<2>)
    );

    BOOST_HANA_CONSTANT_CHECK(hana::front(hana::tuple_c<int, 0, 1, 2>) == hana::int_c<0>);
}
