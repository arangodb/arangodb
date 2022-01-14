// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/integral_c.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/not_equal.hpp>

#include <boost/mpl/integral_c.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


int main() {
    // Interoperation with hana::integral_constant
    BOOST_HANA_CONSTANT_CHECK(mpl::integral_c<int, 1>{} == hana::int_c<1>);
    BOOST_HANA_CONSTANT_CHECK(mpl::integral_c<int, 1>{} == hana::long_c<1>);

    BOOST_HANA_CONSTANT_CHECK(mpl::integral_c<int, 2>{} != hana::int_c<3>);
}
