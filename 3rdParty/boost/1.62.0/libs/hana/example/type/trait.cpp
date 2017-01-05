// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::trait<std::is_integral>(hana::type_c<int>));
BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::trait<std::is_integral>(hana::type_c<float>)));

int main() { }
