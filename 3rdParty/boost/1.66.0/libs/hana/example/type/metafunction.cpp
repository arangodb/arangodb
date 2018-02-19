// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <typename ...> struct f { struct type; };
struct x;
struct y;

BOOST_HANA_CONSTANT_CHECK(hana::metafunction<f>() == hana::type_c<f<>::type>);
BOOST_HANA_CONSTANT_CHECK(hana::metafunction<f>(hana::type_c<x>) == hana::type_c<f<x>::type>);
BOOST_HANA_CONSTANT_CHECK(hana::metafunction<f>(hana::type_c<x>, hana::type_c<y>) == hana::type_c<f<x, y>::type>);

static_assert(std::is_same<
    decltype(hana::metafunction<f>)::apply<x, y>::type,
    f<x, y>::type
>::value, "");

int main() { }
