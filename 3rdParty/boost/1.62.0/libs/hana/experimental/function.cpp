// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/all_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/demux.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/transform.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;
using namespace hana::literals;


struct Function { };

template <typename Domain, typename Codomain, typename F>
struct function_type {
    using hana_tag = Function;

    Domain domain_;
    Codomain codomain_;
    F f_;

    template <typename X>
    constexpr auto operator()(X x) const {
        BOOST_HANA_ASSERT(boost::hana::contains(domain_, x));
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


// BOOST_HANA_CONSTEXPR_LAMBDA auto is_injective = [](auto f) {
//     auto check = [](auto x, auto y) {
//         return (x != y)     ^implies^   (f(x) != f(y));
//     };
//     return all_of(product(domain(f), domain(f)), check);
// };

// BOOST_HANA_CONSTEXPR_LAMBDA auto is_onto = [](auto f) {
//     return codomain(f) == range(g);
// };

//////////////////////////////////////////////////////////////////////////////

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
}
