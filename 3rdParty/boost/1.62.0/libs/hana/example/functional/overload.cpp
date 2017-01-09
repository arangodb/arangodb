// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/functional/overload.hpp>

#include <iostream>
#include <string>
namespace hana = boost::hana;


auto on_string = [](std::string const& s) {
    std::cout << "matched std::string: " << s << std::endl;
    return s;
};

auto on_int = [](int i) {
    std::cout << "matched int: " << i << std::endl;
    return i;
};

auto f = hana::overload(on_int, on_string);

int main() {
    // prints "matched int: 1"
    BOOST_HANA_RUNTIME_CHECK(f(1) == 1);

    // prints "matched std::string: abcdef"
    BOOST_HANA_RUNTIME_CHECK(f("abcdef") == std::string{"abcdef"});
}
