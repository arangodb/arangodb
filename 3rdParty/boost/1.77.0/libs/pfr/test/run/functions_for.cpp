// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/functions_for.hpp>

#include <boost/core/lightweight_test.hpp>

#include <iostream>
#include <typeinfo>
#include <tuple>
#include <sstream>
#include <set>
#include <string>

#include <boost/functional/hash.hpp>
#include <unordered_set>

struct adl_hash {
    template <class T>
    std::size_t operator()(const T& val) const {
        using namespace boost;
        return hash_value(val);
    }
};

struct comparable_struct {
    int i; short s; bool bl; int a,b,c,d,e,f;
};

BOOST_PFR_FUNCTIONS_FOR(comparable_struct)

template <typename Struct>
void test_some_comparable_struct() {
    Struct s1 {0, 1, false, 6,7,8,9,10,11};
    Struct s2 = s1;
    Struct s3 {0, 1, false, 6,7,8,9,10,11111};
    BOOST_TEST_EQ(s1, s2);
    BOOST_TEST(s1 <= s2);
    BOOST_TEST(s1 >= s2);
    BOOST_TEST(!(s1 != s2));
    BOOST_TEST(!(s1 == s3));
    BOOST_TEST(s1 != s3);
    BOOST_TEST(s1 < s3);
    BOOST_TEST(s3 > s2);
    BOOST_TEST(s1 <= s3);
    BOOST_TEST(s3 >= s2);

    std::cout << s1 << std::endl;

    Struct s4;
    std::stringstream ss;
    ss.exceptions ( std::ios::failbit);
    ss << s1;
    ss >> s4;
    std::cout << s4 << std::endl;
    BOOST_TEST_EQ(s1, s4);
    int i = 1, j = 2;
    BOOST_TEST_NE(i, j);
}

void test_comparable_struct() {
    test_some_comparable_struct<comparable_struct>();
}

struct empty { operator std::string() { return "empty{}"; } };
BOOST_PFR_FUNCTIONS_FOR(empty)

void test_empty_struct() {
    BOOST_TEST_EQ(empty{}, empty{});
}

namespace foo {
    struct testing { bool b1, b2; int i; };
    BOOST_PFR_FUNCTIONS_FOR(testing)
}

template <class Comparator>
void test_with_contatiners() {
    std::set<foo::testing, Comparator > t{
        {true, true, 100},
        {false, true, 100},
        {true, false, 100},
        {true, true, 101}
    };

    BOOST_TEST(t.find({true, true, 100}) != t.end());
    BOOST_TEST_EQ(t.count({true, true, 100}), 1u);

    std::unordered_set<foo::testing, adl_hash> us(t.begin(), t.end());
    BOOST_TEST_EQ(us.size(), t.size());
}

void test_implicit_conversions() {
    std::stringstream ss;
    ss << std::true_type{};
    BOOST_TEST_EQ(ss.str(), "1"); // Does not breaks implicit conversion

    ss.str("");
    ss << empty{};
    BOOST_TEST_EQ(ss.str(), "{}"); // Breaks implicit conversion for types marked with BOOST_PFR_FUNCTIONS_FOR
}

namespace {

struct anonymous_comparable_struct {
    int i; short s; bool bl; int a,b,c,d,e,f;
};

BOOST_PFR_FUNCTIONS_FOR(anonymous_comparable_struct)


struct other_anonymous_struct {
    anonymous_comparable_struct a,b;
};

BOOST_PFR_FUNCTIONS_FOR(other_anonymous_struct)

}

namespace std {
template <>
struct hash<anonymous_comparable_struct> {
    std::size_t operator()(const anonymous_comparable_struct& val) const noexcept {
        return hash_value(val);
    }
};
}

namespace {

void test_anonymous_comparable_struct() {
    test_some_comparable_struct<anonymous_comparable_struct>();
}

void test_nested_anonymous_comparable_struct() {
    other_anonymous_struct s1{
        {0, 1, false, 6,7,8,9,10,11},
        {0, 1, false, 6,7,8,9,10,11},
    };
    auto s2 = s1;

    BOOST_TEST_EQ(s1, s2);
}

}

int main() {
    test_comparable_struct();
    test_empty_struct();
    test_with_contatiners<std::less<>>();
    test_with_contatiners<std::greater<>>();
    test_implicit_conversions();
    test_anonymous_comparable_struct();
    test_nested_anonymous_comparable_struct();
    return boost::report_errors();
}


