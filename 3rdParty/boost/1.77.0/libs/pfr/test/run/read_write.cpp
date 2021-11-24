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



struct with_operator{};
inline bool operator==(with_operator, with_operator) {
    return true;
}
std::ostream& operator<<(std::ostream& os, with_operator) {
    return os << "{with_operator}";
}
std::istream& operator>>(std::istream& is, with_operator&) {
    std::string s;
    is >> s;
    return is;
}

int main() {
    struct test1 {
        int f0;
        int f1;
        char f2;
        int f3;
        short f4;
    };
    test_type(test1{1, 2, '3', 4, 5}, "{1, 2, 3, 4, 5}");
    test_type(test1{199, 299, '9', 499, 599}, "{199, 299, 9, 499, 599}");

    struct test2 {
        with_operator f0;
        with_operator f1;
        with_operator f2;
        with_operator f3;
        with_operator f4;
    };
    test_type(test2{}, "{{with_operator}, {with_operator}, {with_operator}, {with_operator}, {with_operator}}");

    struct test3 {
        int f0;
        int f1;
        char f2;
        int f3;
        with_operator f4;
    };

    test_type(
        test3{1, 2, '3', 4, {}},
        "{1, 2, 3, 4, {with_operator}}"
    );

    return boost::report_errors();
}


