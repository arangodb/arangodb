// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/make.hpp>
namespace hana = boost::hana;


struct udt {
    int value;
    constexpr explicit udt(int v) : value(v) { }
};

static_assert(hana::make<udt>(1).value == 1, "");

int main() { }
