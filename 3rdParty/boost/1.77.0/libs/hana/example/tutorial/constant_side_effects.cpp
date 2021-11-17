// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>

#include <iostream>
namespace hana = boost::hana;


namespace pure {
//! [pure]
template <typename X>
auto identity(X x) { return x; }

auto x = identity(hana::bool_c<true>);
static_assert(hana::value(x), "");
//! [pure]
}

namespace impure {
//! [impure_identity]
template <typename X>
auto identity(X x) {
    std::cout << "Good luck in evaluating this at compile-time!";
    return x;
}
//! [impure_identity]

//! [impure]
auto x = identity(hana::bool_c<true>);
static_assert(hana::value(x), "");
//! [impure]
}

int main() { }
