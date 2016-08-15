// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
namespace hana = boost::hana;


template <bool value>
bool runtime_bool() { return value; }


int main() {
    // Make sure it works at function scope
    BOOST_HANA_RUNTIME_ASSERT(runtime_bool<true>());
    BOOST_HANA_RUNTIME_ASSERT_MSG(runtime_bool<true>(), "message");

    // Make sure we can reference a local variable
    auto yes = runtime_bool<true>();
    BOOST_HANA_RUNTIME_ASSERT(yes);
    BOOST_HANA_RUNTIME_ASSERT_MSG(yes, "message");
}
