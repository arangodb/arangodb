// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <string>

#include <boost/pfr/ops.hpp>

union test_union {
    const char* c;
    int i;
};

int main() {
    struct two_unions {
        test_union u1, u2;
    };

    two_unions v{{""}, {""}};
    return boost::pfr::eq(v, v);
}
