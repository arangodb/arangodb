// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/metafunction.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct x1; struct x2; struct x3;
struct y1 { }; struct y2 { }; struct y3 { };
struct f { template <typename ...> struct apply { struct type; }; };

template <typename F, typename ...T>
constexpr auto valid_call(F f, T ...t) -> decltype(((void)f(t...)), true)
{ return true; }
constexpr auto valid_call(...)
{ return false; }

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction_class<f>(),
    hana::type_c<f::apply<>::type>
));
BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction_class<f>(hana::type_c<x1>),
    hana::type_c<f::apply<x1>::type>
));
BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction_class<f>(hana::type_c<x1>, hana::type_c<x2>),
    hana::type_c<f::apply<x1, x2>::type>
));
BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction_class<f>(hana::type_c<x1>, hana::type_c<x2>, hana::type_c<x3>),
    hana::type_c<f::apply<x1, x2, x3>::type>
));

using F = decltype(hana::metafunction_class<f>);
static_assert(std::is_same<F::apply<>, f::apply<>>{}, "");
static_assert(std::is_same<F::apply<x1>, f::apply<x1>>{}, "");
static_assert(std::is_same<F::apply<x1, x2>, f::apply<x1, x2>>{}, "");
static_assert(std::is_same<F::apply<x1, x2, x3>, f::apply<x1, x2, x3>>{}, "");

// Make sure we're SFINAE-friendly
struct no_type { template <typename ...> struct apply { }; };
static_assert(!valid_call(hana::metafunction_class<no_type>), "");
static_assert(!valid_call(hana::metafunction_class<no_type>, hana::type_c<x1>), "");

// Make sure we model the Metafunction concept
static_assert(hana::Metafunction<decltype(hana::metafunction_class<f>)>::value, "");
static_assert(hana::Metafunction<decltype(hana::metafunction_class<f>)&>::value, "");


// Make sure we don't read from a non-constexpr variable
int main() {
    auto t = hana::type_c<x1>;
    constexpr auto r = hana::metafunction_class<f>(t);
    (void)r;
}
