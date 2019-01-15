// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/all_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/cartesian_product.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/demux.hpp>
#include <boost/hana/fuse.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/or.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;
using namespace hana::literals;


// A function that can have an arbitrary compile-time set of values as a domain
// and co-domain. This is most likely purely of theoretical interest, but it
// allows creating functions with very complex domains and co-domains that are
// computed at compile-time.

struct Function { };

template <typename Domain, typename Codomain, typename F>
struct function_type {
    using hana_tag = Function;

    Domain domain_;
    Codomain codomain_;
    F f_;

    template <typename X>
    constexpr auto operator()(X x) const {
        BOOST_HANA_CONSTANT_ASSERT(boost::hana::contains(domain_, x));
        return f_(x);
    }
};

template <typename ...F, typename ...G>
constexpr auto operator==(function_type<F...> f, function_type<G...> g)
{ return hana::equal(f, g); }

template <typename ...F, typename ...G>
constexpr auto operator!=(function_type<F...> f, function_type<G...> g)
{ return hana::not_equal(f, g); }


auto function = [](auto domain, auto codomain) {
    return [=](auto definition) {
        return function_type<decltype(domain), decltype(codomain), decltype(definition)>{
            domain, codomain, definition
        };
    };
};

template <typename Function>
constexpr auto domain(Function f)
{ return f.domain_; }

template <typename Function>
constexpr auto codomain(Function f)
{ return f.codomain_; }

template <typename Function>
constexpr auto range(Function f) {
    // We must convert to hana::tuple first because hana::set is not a Functor
    return hana::to_set(hana::transform(hana::to_tuple(domain(f)), f));
}

template <typename P, typename Q>
constexpr auto implies(P p, Q q) {
    return hana::or_(hana::not_(p), q);
}

template <typename F>
constexpr auto is_injective(F f) {
    auto dom = hana::to_tuple(domain(f));
    auto pairs = hana::cartesian_product(hana::make_tuple(dom, dom));
    return hana::all_of(pairs, hana::fuse([&](auto x, auto y) {
        return implies(hana::not_equal(x, y), hana::not_equal(f(x), f(y)));
    }));
}

template <typename F>
constexpr auto is_onto(F f) {
    return codomain(f) == range(f);
}

namespace boost { namespace hana {
    template <>
    struct equal_impl<Function, Function> {
        template <typename F, typename G>
        static constexpr auto apply(F f, G g) {
            return domain(f) == domain(g) &&
                   hana::all_of(domain(f), hana::demux(hana::equal)(f, g));
        }
    };
}} // end namespace boost::hana

int main() {
    auto f = function(hana::make_set(1_c, 2_c, 3_c), hana::make_set(1_c, 2_c, 3_c, 4_c, 5_c, 6_c))(
        [](auto x) { return x + 1_c; }
    );

    auto g = function(hana::make_set(1_c, 2_c, 3_c), hana::make_set(2_c, 3_c, 4_c))(
        [](auto x) { return x + 1_c; }
    );

    auto h = function(hana::make_set(1_c, 2_c, 3_c), hana::make_set(0_c, 1_c, 2_c))(
        [](auto x) { return x - 1_c; }
    );

    BOOST_HANA_CONSTANT_CHECK(f == g);
    BOOST_HANA_CONSTANT_CHECK(f != h);
    BOOST_HANA_CONSTANT_CHECK(f(1_c) == 2_c);

    BOOST_HANA_CONSTANT_CHECK(range(f) == hana::make_set(4_c, 3_c, 2_c));
    BOOST_HANA_CONSTANT_CHECK(range(g) == hana::make_set(2_c, 3_c, 4_c));
    BOOST_HANA_CONSTANT_CHECK(range(h) == hana::make_set(0_c, 1_c, 2_c));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(is_onto(f)));
    BOOST_HANA_CONSTANT_CHECK(is_onto(g));
    BOOST_HANA_CONSTANT_CHECK(is_onto(h));

    auto even = function(hana::make_set(1_c, 2_c, 3_c), hana::make_set(hana::true_c, hana::false_c))(
        [](auto x) { return x % 2_c == 0_c; }
    );

    BOOST_HANA_CONSTANT_CHECK(is_injective(f));
    BOOST_HANA_CONSTANT_CHECK(is_injective(g));
    BOOST_HANA_CONSTANT_CHECK(is_injective(h));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(is_injective(even)));
}
