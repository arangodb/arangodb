// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>

#include <memory>
namespace hana = boost::hana;


int main() {
    auto map = hana::make_map(
        hana::make_pair(hana::int_c<0>, 123),
        hana::make_pair(hana::int_c<1>, 456)
    );

    std::unique_ptr<decltype(map)> p1{}, p2{};
    using std::swap;
    swap(p1, p2);
}
