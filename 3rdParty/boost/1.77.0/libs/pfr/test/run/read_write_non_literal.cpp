// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/io.hpp>

#include <sstream>
#include <string>

#include <boost/core/lightweight_test.hpp>

template <class T>
void test_write_read(const T& value) {
    T result;
    std::stringstream ss;
    ss << boost::pfr::io(value);
    ss >> boost::pfr::io(result);
    BOOST_TEST_EQ(value.f0, result.f0);
    BOOST_TEST_EQ(value.f1, result.f1);
    BOOST_TEST_EQ(value.f2, result.f2);
    BOOST_TEST_EQ(value.f3, result.f3);
    BOOST_TEST_EQ(value.f4, result.f4);
}

template <class T>
void to_string_test(const T& value, const char* ethalon) {
    std::stringstream ss;
    ss << boost::pfr::io(value);
    BOOST_TEST_EQ(ss.str(), ethalon);
}

template <class T>
void test_type(const T& value, const char* ethalon) {
    test_write_read(value);
    to_string_test(value, ethalon);
}


int main() {
    struct test4 {
        int f0;
        std::string f1;
        char f2;
        int f3;
        std::string f4;
    };
    test_type(
        test4{1, {"my o my"}, '3', 4, {"hello there!"} },
        "{1, \"my o my\", 3, 4, \"hello there!\"}"
    );

    #if 0
    // TODO:
    std::string f1_referenced{"my O my"};
    std::string f4_referenced{"Hello There!"};
    struct test5 {
        int f0;
        const std::string& f1;
        char f2;
        int f3;
        const std::string& f4;
    };
    to_string_test(
        test5{1, f1_referenced, '3', 4, f4_referenced },
        "{1, \"my o my\", 3, 4, \"hello there!\"}"
    );
    #endif

    return boost::report_errors();
}


