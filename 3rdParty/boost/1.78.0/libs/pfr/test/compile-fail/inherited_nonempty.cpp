// Copyright (c) 2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>

struct A {
    int zero;
};

struct B : public A {
    int one;
    int two;
};

int main() {
    (void)boost::pfr::tuple_size<B>::value; // Must be a compile time error
}

