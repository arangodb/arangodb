// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/is_a.hpp>
namespace hana = boost::hana;


int main() {
    // This used to trigger a compilation error on Clang 3.5.
    {
        auto f = [](auto i) {
            constexpr auto result = hana::is_an<int>(i);
            (void)result;
        };
        f(1);
    }
}
