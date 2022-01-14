// Copyright (c) 2018-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Test case for https://github.com/apolukhin/magic_get/issues/33

#include <iostream>
#include <vector>
#include <boost/pfr.hpp>
#include <boost/core/lightweight_test.hpp>

struct TestStruct {
    std::vector<std::unique_ptr<int>> vec;
};

int main() {
    TestStruct temp;
    temp.vec.emplace_back();

    boost::pfr::for_each_field(temp, [](const auto& value) {
        BOOST_TEST_EQ(value.size(), 1);
    });

    return boost::report_errors();
}
