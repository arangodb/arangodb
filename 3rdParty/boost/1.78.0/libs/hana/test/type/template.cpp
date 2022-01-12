// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/metafunction.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct x1; struct x2; struct x3;
struct y1 { }; struct y2 { }; struct y3 { };
template <typename ...> struct f;

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::template_<f>(),
    hana::type_c<f<>>
));
BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::template_<f>(hana::type_c<x1>),
    hana::type_c<f<x1>>
));
BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::template_<f>(hana::type_c<x1>, hana::type_c<x2>),
    hana::type_c<f<x1, x2>>
));
BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::template_<f>(hana::type_c<x1>, hana::type_c<x2>, hana::type_c<x3>),
    hana::type_c<f<x1, x2, x3>>
));

using F = decltype(hana::template_<f>);
static_assert(std::is_same<F::apply<>::type, f<>>{}, "");
static_assert(std::is_same<F::apply<x1>::type, f<x1>>{}, "");
static_assert(std::is_same<F::apply<x1, x2>::type, f<x1, x2>>{}, "");
static_assert(std::is_same<F::apply<x1, x2, x3>::type, f<x1, x2, x3>>{}, "");

// Make sure we model the Metafunction concept
static_assert(hana::Metafunction<decltype(hana::template_<f>)>::value, "");
static_assert(hana::Metafunction<decltype(hana::template_<f>)&>::value, "");

// Make sure we can use aliases
template <typename T> using alias = T;
static_assert(hana::template_<alias>(hana::type_c<x1>) == hana::type_c<x1>, "");

// Make sure template_ is SFINAE-friendly
template <typename T> struct unary;
BOOST_HANA_CONSTANT_CHECK(hana::not_(
    hana::is_valid(hana::template_<unary>)(hana::type_c<void>, hana::type_c<void>)
));

// Make sure we don't read from a non-constexpr variable
int main() {
    auto t = hana::type_c<x1>;
    constexpr auto r = hana::template_<f>(t);
    (void)r;
}
