// Copyright (c) 2020-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>

#include <memory>

#include <boost/core/lightweight_test.hpp>

struct unique_ptrs {
    std::unique_ptr<int> p1;
    std::unique_ptr<int> p2;
};

void test_get_rvalue() {
    unique_ptrs x {
        std::make_unique<int>(42),
        std::make_unique<int>(43),
    };

    auto p = boost::pfr::get<0>(std::move(x));
    BOOST_TEST_EQ(*p, 42);
}

int main() {
    test_get_rvalue();

    return boost::report_errors();
}
