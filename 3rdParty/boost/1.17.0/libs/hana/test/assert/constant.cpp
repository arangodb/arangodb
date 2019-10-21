// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>

#include <support/cnumeric.hpp>
namespace hana = boost::hana;


template <bool value>
auto constant_bool() { return make_cnumeric<bool, value>(); }

template <bool value>
constexpr auto constexpr_constant_bool() { return make_cnumeric<bool, value>(); }


// Make sure it works at global scope
BOOST_HANA_CONSTANT_ASSERT(constant_bool<true>());
BOOST_HANA_CONSTANT_ASSERT_MSG(constant_bool<true>(), "message");

// Make sure it works at namespace scope
namespace ns {
    BOOST_HANA_CONSTANT_ASSERT(constant_bool<true>());
    BOOST_HANA_CONSTANT_ASSERT_MSG(constant_bool<true>(), "message");
}

// Make sure it works in a constexpr context
constexpr bool constexpr_context() {
    BOOST_HANA_CONSTANT_ASSERT(constexpr_constant_bool<true>());
    BOOST_HANA_CONSTANT_ASSERT_MSG(constexpr_constant_bool<true>(), "message");
    return true;
}
static_assert(constexpr_context(), "");


int main() {
    // Make sure it works at function scope
    BOOST_HANA_CONSTANT_ASSERT(constant_bool<true>());
    BOOST_HANA_CONSTANT_ASSERT_MSG(constant_bool<true>(), "message");

    // Make sure it works inside a lambda
    auto lambda = []{
        BOOST_HANA_CONSTANT_ASSERT(constant_bool<true>());
        BOOST_HANA_CONSTANT_ASSERT_MSG(constant_bool<true>(), "message");
    };
    lambda();

    // Make sure we can reference a local variable
    auto yes = constant_bool<true>();
    BOOST_HANA_CONSTANT_ASSERT(yes);
    BOOST_HANA_CONSTANT_ASSERT_MSG(yes, "message");
}
