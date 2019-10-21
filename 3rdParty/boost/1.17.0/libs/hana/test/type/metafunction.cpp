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
template <typename ...> struct f { struct type; };

template <typename F, typename ...T>
constexpr auto valid_call(F f, T ...t) -> decltype(((void)f(t...)), true)
{ return true; }
constexpr auto valid_call(...)
{ return false; }

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction<f>(),
    hana::type_c<f<>::type>
));

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction<f>(hana::type_c<x1>),
    hana::type_c<f<x1>::type>
));

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction<f>(hana::type_c<x1>, hana::type_c<x2>),
    hana::type_c<f<x1, x2>::type>
));

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::metafunction<f>(hana::type_c<x1>, hana::type_c<x2>, hana::type_c<x3>),
    hana::type_c<f<x1, x2, x3>::type>
));

using F = decltype(hana::metafunction<f>);
static_assert(std::is_same<F::apply<>, f<>>::value, "");
static_assert(std::is_same<F::apply<x1>, f<x1>>::value, "");
static_assert(std::is_same<F::apply<x1, x2>, f<x1, x2>>::value, "");
static_assert(std::is_same<F::apply<x1, x2, x3>, f<x1, x2, x3>>::value, "");

// Make sure we're SFINAE-friendly
template <typename ...T> struct no_type { };
static_assert(!valid_call(hana::metafunction<no_type>), "");
static_assert(!valid_call(hana::metafunction<no_type>, hana::type_c<x1>), "");

// Make sure we model the Metafunction concept
static_assert(hana::Metafunction<decltype(hana::metafunction<f>)>::value, "");
static_assert(hana::Metafunction<decltype(hana::metafunction<f>)&>::value, "");

// Make sure metafunction is SFINAE-friendly
template <typename T> struct not_a_metafunction { };
BOOST_HANA_CONSTANT_CHECK(hana::not_(
    hana::is_valid(hana::metafunction<not_a_metafunction>)(hana::type_c<void>)
));


// Make sure we don't read from a non-constexpr variable
int main() {
    auto t = hana::type_c<x1>;
    constexpr auto r = hana::metafunction<f>(t);
    (void)r;
}
