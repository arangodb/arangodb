// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>

int main() {
    struct nested { int i; char data[20]; };
    struct foo { int i; char c; nested n; };
    static_assert(boost::pfr::tuple_size_v<foo> == 3, "");

    struct with_reference { int& i; char data; };
    static_assert(boost::pfr::tuple_size_v<with_reference> == 2, "");

    struct empty {};
    static_assert(boost::pfr::tuple_size_v<empty> == 0, "");
}

