// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/tuple_size.hpp>

struct X {
    X() = default;
    X(X&&) = default;
    X(const X&) = delete;

    X& operator=(X&&) = default;
    X& operator=(const X&) = delete;
};

struct test_lvalue_ref_and_movable {
    X x;
    char& c;
};

int main() {
    return boost::pfr::tuple_size<test_lvalue_ref_and_movable>::value;
}

