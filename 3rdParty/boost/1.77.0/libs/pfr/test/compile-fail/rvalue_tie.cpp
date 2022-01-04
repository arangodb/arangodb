// Copyright (c) 2020-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>

#include <string>

struct test_struct {
    int i;
    std::string s;
};

int main() {
    boost::pfr::structure_tie(test_struct{1, "test"});
}
