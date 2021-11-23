// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/tuple_size.hpp>

struct bf {
    unsigned int i1: 1;
    unsigned int i2: 1;
    unsigned int i3: 1;
    unsigned int i4: 1;
    unsigned int i5: 1;
    unsigned int i6: 1;
};

int main() {
    static_assert(boost::pfr::tuple_size<bf>::value == 6, "");
}


