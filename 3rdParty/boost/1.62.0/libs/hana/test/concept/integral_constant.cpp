// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/concept/constant.hpp>
#include <boost/hana/concept/integral_constant.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/fwd/core/to.hpp>
#include <boost/hana/value.hpp>
namespace hana = boost::hana;


// Define a simple model of IntegralConstant
struct constant_tag { using value_type = int; };
template <int i>
struct constant {
    static constexpr int value = i;
    using hana_tag = constant_tag;
};

namespace boost { namespace hana {
    template <>
    struct IntegralConstant<constant_tag> {
        static constexpr bool value = true;
    };

    template <typename From>
    struct to_impl<constant_tag, From, when<IntegralConstant<From>::value>> {
        template <typename N>
        static constexpr auto apply(N const&)
        { return constant<N::value>{}; }
    };
}}

// Make sure we really satisfy IntegralConstant<>.
static_assert(hana::IntegralConstant<constant<0>>::value, "");
static_assert(hana::IntegralConstant<constant<1>>::value, "");
static_assert(hana::IntegralConstant<constant<2>>::value, "");

// Make sure we're also a model of Constant automatically.
static_assert(hana::Constant<constant<0>>::value, "");
static_assert(hana::Constant<constant<1>>::value, "");
static_assert(hana::Constant<constant<2>>::value, "");

// Make sure we have the hana::value<> function defined automatically.
static_assert(hana::value<constant<0>>() == 0, "");
static_assert(hana::value<constant<1>>() == 1, "");
static_assert(hana::value<constant<2>>() == 2, "");
static_assert(hana::value<constant<3>>() == 3, "");


int main() { }
