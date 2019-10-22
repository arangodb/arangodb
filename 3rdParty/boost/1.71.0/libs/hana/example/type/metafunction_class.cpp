// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct f { template <typename ...> struct apply { struct type; }; };
struct x;
struct y;

BOOST_HANA_CONSTANT_CHECK(hana::metafunction_class<f>() == hana::type_c<f::apply<>::type>);
BOOST_HANA_CONSTANT_CHECK(hana::metafunction_class<f>(hana::type_c<x>) == hana::type_c<f::apply<x>::type>);
BOOST_HANA_CONSTANT_CHECK(hana::metafunction_class<f>(hana::type_c<x>, hana::type_c<y>) == hana::type_c<f::apply<x, y>::type>);

static_assert(std::is_same<
    decltype(hana::metafunction_class<f>)::apply<x, y>::type,
    f::apply<x, y>::type
>::value, "");

int main() { }
