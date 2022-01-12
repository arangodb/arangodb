// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/tuple_size.hpp>

struct test_with_virtual {
    int i = 0;
    char c = 'a';
    double d = 3.4;
    float f = 3.5f;

    virtual double sum() const { return i + d + c + f; }
};

int main() {
    return boost::pfr::tuple_size<test_with_virtual>::value;
}

