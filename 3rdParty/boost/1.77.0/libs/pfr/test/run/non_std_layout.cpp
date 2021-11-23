// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/tuple_size.hpp>


struct non_standard_layout_member {
private:
    int i = 0;

public:
    int j = 1;
};

struct test_with_non_st_layout {
    non_standard_layout_member m;
    double d;
    float f;
};

int main() {
    static_assert(boost::pfr::tuple_size<test_with_non_st_layout>::value == 3, "");
}

