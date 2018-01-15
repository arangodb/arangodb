// Copyright Louis Dionne 2013-2017
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

template <typename ...> struct mf { struct type { }; };
struct mfc { template <typename ...> struct apply { struct type { }; }; };
template <typename ...> struct tpl { };

// make sure `integral(f)(...)` returns the right type
static_assert(std::is_same<
    decltype(hana::integral(hana::metafunction<mf>)()),
    mf<>::type
>{}, "");
static_assert(std::is_same<
    decltype(hana::integral(hana::metafunction<mf>)(hana::type_c<x1>)),
    mf<x1>::type
>{}, "");
static_assert(std::is_same<
    decltype(hana::integral(hana::metafunction<mf>)(hana::type_c<x1>, hana::type_c<x2>)),
    mf<x1, x2>::type
>{}, "");

static_assert(std::is_same<
    decltype(hana::integral(hana::template_<tpl>)()),
    tpl<>
>{}, "");
static_assert(std::is_same<
    decltype(hana::integral(hana::template_<tpl>)(hana::type_c<x1>)),
    tpl<x1>
>{}, "");
static_assert(std::is_same<
    decltype(hana::integral(hana::template_<tpl>)(hana::type_c<x1>, hana::type_c<x2>)),
    tpl<x1, x2>
>{}, "");

static_assert(std::is_same<
    decltype(hana::integral(hana::metafunction_class<mfc>)()),
    mfc::apply<>::type
>{}, "");
static_assert(std::is_same<
    decltype(hana::integral(hana::metafunction_class<mfc>)(hana::type_c<x1>)),
    mfc::apply<x1>::type
>{}, "");
static_assert(std::is_same<
    decltype(hana::integral(hana::metafunction_class<mfc>)(hana::type_c<x1>, hana::type_c<x2>)),
    mfc::apply<x1, x2>::type
>{}, "");


int main() {
    // Make sure we can perform the call; we already made sure the return type was correct
    constexpr auto a = hana::integral(hana::metafunction<mf>)(); (void)a;
    constexpr auto b = hana::integral(hana::metafunction<mf>)(hana::type_c<x1>); (void)b;
    constexpr auto c = hana::integral(hana::metafunction<mf>)(hana::type_c<x1>, hana::type_c<x2>); (void)c;
    constexpr auto d = hana::integral(hana::metafunction<mf>)(hana::type_c<x1>, hana::type_c<x2>, hana::type_c<x3>); (void)d;

    // Make sure we don't read from a non-constexpr variable
    auto t = hana::type_c<x1>;
    constexpr auto r = hana::integral(hana::metafunction<mf>)(t);
    (void)r;
}
