// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/functional/overload_linearly.hpp>

#include <string>
namespace hana = boost::hana;


auto f = hana::overload_linearly(
    [](int i) { return i + 1; },
    [](std::string s) { return s + "d"; },
    [](double) { BOOST_HANA_RUNTIME_CHECK(false && "never called"); }
);

int main() {
    BOOST_HANA_RUNTIME_CHECK(f(1) == 2);
    BOOST_HANA_RUNTIME_CHECK(f("abc") == "abcd");
    BOOST_HANA_RUNTIME_CHECK(f(2.2) == static_cast<int>(2.2) + 1);
}
