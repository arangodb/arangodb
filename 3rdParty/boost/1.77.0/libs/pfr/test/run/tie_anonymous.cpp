// Copyright (c) 2020-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>

#include <boost/pfr/core.hpp>

#include <string>
#include <type_traits>

#if defined(__has_include)
#   if __has_include(<optional>) && (__cplusplus >= 201703L)
#       include <optional>
#       ifdef __cpp_lib_optional
#           define BOOST_PFR_TEST_HAS_OPTIONAL 1
#       endif
#   endif
#endif

#ifndef BOOST_PFR_TEST_HAS_OPTIONAL
#define BOOST_PFR_TEST_HAS_OPTIONAL 0
#endif

namespace some {
    struct struct1{ int i; };
    struct struct2{ int i; };
}

namespace testing {

namespace {

#if BOOST_PFR_TEST_HAS_OPTIONAL
struct anon_with_optional {
    std::string a;
    std::optional<some::struct1> b;
    std::optional<some::struct2> c;

};

struct other_anon_with_optional {
    std::string a;
    int b;
    std::optional<anon_with_optional> c;
    std::optional<some::struct2> d;

};
#endif

struct other_anon {
    int data;
};

struct anon {
    other_anon a;
    const other_anon b;
};

void test_in_anon_ns() {
    const anon const_x{{10}, {20}};

    auto const_v = boost::pfr::structure_tie(const_x);
    BOOST_TEST_EQ(std::get<0>(const_v).data, 10);
    BOOST_TEST_EQ(std::get<1>(const_v).data, 20);
    static_assert(std::is_same<
        std::tuple<const other_anon&, const other_anon&>, decltype(const_v)
    >::value, "");

    // TODO: something is wrong with loophole and optional
#if BOOST_PFR_TEST_HAS_OPTIONAL && !BOOST_PFR_USE_LOOPHOLE
    other_anon_with_optional opt{"test", {}, {}, {}};
    auto opt_val = boost::pfr::structure_tie(opt);
    BOOST_TEST_EQ(std::get<0>(opt_val), "test");
#endif
}

} // anonymous namespace

void test_in_non_non_ns() {
    const anon const_x{{10}, {20}};

    auto const_v = boost::pfr::structure_tie(const_x);
    BOOST_TEST_EQ(std::get<0>(const_v).data, 10);
    BOOST_TEST_EQ(std::get<1>(const_v).data, 20);
    static_assert(std::is_same<
        std::tuple<const other_anon&, const other_anon&>, decltype(const_v)
    >::value, "");

    // TODO: something is wrong with loophole and optional
#if BOOST_PFR_TEST_HAS_OPTIONAL && !BOOST_PFR_USE_LOOPHOLE
    other_anon_with_optional opt{"test again", {}, {}, {}};
    auto opt_val = boost::pfr::structure_tie(opt);
    BOOST_TEST_EQ(std::get<0>(opt_val), "test again");
#endif
}

} // namespace testing

int main() {
    testing::test_in_anon_ns();
    testing::test_in_non_non_ns();

    return boost::report_errors();
}


