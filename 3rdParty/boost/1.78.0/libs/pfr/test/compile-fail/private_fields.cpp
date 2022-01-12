// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/tuple_size.hpp>

#ifdef __clang__
#   pragma clang diagnostic ignored "-Wunused-private-field"
#endif


class test_with_private {
private:
    int i;
    char c;

public:
    double d;
    float f;
};

int main() {
#ifndef __cpp_lib_is_aggregate
#   error No known way to detect private fields.
#endif

    return boost::pfr::tuple_size<test_with_private>::value;
}

