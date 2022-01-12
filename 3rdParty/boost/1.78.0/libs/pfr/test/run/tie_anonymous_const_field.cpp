// Copyright (c) 2020-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>

#include <boost/pfr/core.hpp>

#include <string>
#include <typeindex>
#include <type_traits>

namespace testing {

namespace {

struct other_anon {
    int data;
};

struct anon {
    other_anon a;
    const other_anon b;
};

void test_in_anon_ns_const_field() {
    anon x{{1}, {2}};

    auto v = boost::pfr::structure_tie(x);
    using v_type = decltype(v);
    using expected_type = std::tuple<other_anon&, const other_anon&>;

    // Use runtime check to make sure that Loophole fails to compile structure_tie
    BOOST_TEST(typeid(expected_type) == typeid(v_type));
}

} // anonymous namespace

void test_in_non_non_ns_const_field() {
    anon x{{1}, {2}};

    auto v = boost::pfr::structure_tie(x);
    using v_type = decltype(v);
    using expected_type = std::tuple<other_anon&, const other_anon&>;

    // Use runtime check to make sure that Loophole fails to compile structure_tie
    BOOST_TEST(typeid(expected_type) == typeid(v_type));
}

} // namespace testing

int main() {
    testing::test_in_anon_ns_const_field();
    testing::test_in_non_non_ns_const_field();

    return boost::report_errors();
}


