// Copyright Louis Dionne 2013-2017
// Copyright Jason Rice 2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/index_if.hpp>
#include <boost/hana/integral_constant.hpp>

#include <cstddef>
#include <support/counter.hpp>

namespace hana = boost::hana;


int main() {
    // Tests hana::index_if on an infinite iterable
    constexpr Counter<> c{};
    auto pred = [](auto i) {
        return [=](auto x) {
            return hana::bool_c<decltype(x)::value == decltype(i)::value>;
        };
    };

    static_assert(hana::value(decltype(hana::index_if(c, pred(hana::size_c<0>)).value()){}) == 0, "");
    static_assert(hana::value(decltype(hana::index_if(c, pred(hana::size_c<1>)).value()){}) == 1, "");
    static_assert(hana::value(decltype(hana::index_if(c, pred(hana::size_c<2>)).value()){}) == 2, "");
    static_assert(hana::value(decltype(hana::index_if(c, pred(hana::size_c<3>)).value()){}) == 3, "");
    static_assert(hana::value(decltype(hana::index_if(c, pred(hana::size_c<4>)).value()){}) == 4, "");
    static_assert(hana::value(decltype(hana::index_if(c, pred(hana::size_c<5>)).value()){}) == 5, "");
    static_assert(hana::value(decltype(hana::index_if(c, pred(hana::size_c<6>)).value()){}) == 6, "");
}
