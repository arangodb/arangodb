// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/value.hpp>
namespace hana = boost::hana;


int main() {
    auto i = hana::integral_c<int, 3>; // notice no constexpr
    static_assert(hana::value<decltype(i)>() == 3, "");
    static_assert(hana::value(i) == 3, "value(i) is always a constant expression!");
}
