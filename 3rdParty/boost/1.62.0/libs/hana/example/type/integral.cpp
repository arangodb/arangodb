// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


constexpr auto is_void = hana::integral(hana::metafunction<std::is_void>);
BOOST_HANA_CONSTANT_CHECK(is_void(hana::type_c<void>));
BOOST_HANA_CONSTANT_CHECK(hana::not_(is_void(hana::type_c<int>)));

int main() { }
