// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
namespace hana = boost::hana;


template <bool value>
bool runtime_bool() { return value; }

template <bool value>
constexpr bool constexpr_bool() { return value; }


int main() {
    // Make sure it works at function scope
    BOOST_HANA_CONSTEXPR_ASSERT(runtime_bool<true>());
    BOOST_HANA_CONSTEXPR_ASSERT(constexpr_bool<true>());
    BOOST_HANA_CONSTEXPR_ASSERT_MSG(runtime_bool<true>(), "message");
    BOOST_HANA_CONSTEXPR_ASSERT_MSG(constexpr_bool<true>(), "message");

    // Make sure we can reference a local variable
    auto rt_yes = runtime_bool<true>();
    constexpr auto cx_yes = constexpr_bool<true>();
    BOOST_HANA_CONSTEXPR_ASSERT(rt_yes);
    BOOST_HANA_CONSTEXPR_ASSERT(cx_yes);
    BOOST_HANA_CONSTEXPR_ASSERT_MSG(rt_yes, "message");
    BOOST_HANA_CONSTEXPR_ASSERT_MSG(cx_yes, "message");
}
