// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::length(hana::make_tuple()) == hana::size_c<0>);
    BOOST_HANA_CONSTANT_CHECK(hana::length(hana::make_tuple(1, '2', 3.0)) == hana::size_c<3>);

    BOOST_HANA_CONSTANT_CHECK(hana::length(hana::nothing) == hana::size_c<0>);
    BOOST_HANA_CONSTANT_CHECK(hana::length(hana::just('x')) == hana::size_c<1>);
}
