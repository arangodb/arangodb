// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;


// This used to trigger an ICE on Clang

struct Car { std::string name; };

int main() {
    auto stuff = hana::make_tuple(Car{}, Car{}, Car{});
    hana::any_of(stuff, [](auto&&) { return true; });
}
