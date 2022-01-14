// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/ops.hpp>
#include <boost/pfr/io.hpp>

#include <iostream>
#include <typeinfo>
#include <tuple>
#include <sstream>
#include <set>
#include <string>

#include <boost/core/lightweight_test.hpp>

#ifdef __clang__
#   pragma clang diagnostic ignored "-Wmissing-braces"
#endif

unsigned test_union_counter = 0;

union test_union {
    int i;
    float f;
};

inline bool operator< (test_union l, test_union r) noexcept { ++test_union_counter; return l.i <  r.i; }
inline bool operator<=(test_union l, test_union r) noexcept { ++test_union_counter; return l.i <= r.i; }
inline bool operator> (test_union l, test_union r) noexcept { ++test_union_counter; return l.i >  r.i; }
inline bool operator>=(test_union l, test_union r) noexcept { ++test_union_counter; return l.i >= r.i; }
inline bool operator==(test_union l, test_union r) noexcept { ++test_union_counter; return l.i == r.i; }
inline bool operator!=(test_union l, test_union r) noexcept { ++test_union_counter; return l.i != r.i; }
inline std::ostream& operator<<(std::ostream& os, test_union src) { ++test_union_counter; return os << src.i; }
inline std::istream& operator>>(std::istream& is, test_union& src) { ++test_union_counter; return is >> src.i; }


template <class T>
void test_comparable_struct() {
    using namespace boost::pfr;
    T s1 {0, 1, false, 6,7,8,9,10,11};
    T s2 = s1;
    T s3 {0, 1, false, 6,7,8,9,10,11111};
    BOOST_TEST(eq(s1, s2));
    BOOST_TEST(le(s1, s2));
    BOOST_TEST(ge(s1, s2));
    BOOST_TEST(!ne(s1, s2));
    BOOST_TEST(!eq(s1, s3));
    BOOST_TEST(ne(s1, s3));
    BOOST_TEST(lt(s1, s3));
    BOOST_TEST(gt(s3, s2));
    BOOST_TEST(le(s1, s3));
    BOOST_TEST(ge(s3, s2));

    std::cout << boost::pfr::io(s1);

    T s4;
    std::stringstream ss;
    ss.exceptions ( std::ios::failbit);
    ss << boost::pfr::io(s1);
    ss >> boost::pfr::io(s4);
    std::cout << boost::pfr::io(s4);
    BOOST_TEST(eq(s1, s4));
}

void test_empty_struct() {
    struct empty {};
    std::cout << boost::pfr::io(empty{});
    BOOST_TEST(boost::pfr::eq(empty{}, empty{}));
}

void test_implicit_conversions() {
    std::stringstream ss;
    ss << boost::pfr::io(std::true_type{});
    BOOST_TEST_EQ(ss.str(), "1"); // Does not break implicit conversion
}


namespace foo {
struct comparable_struct {
    int i; short s; bool bl; int a,b,c,d,e,f;
};
}

int main() {
    test_comparable_struct<foo::comparable_struct>();

    struct local_comparable_struct {
        int i; short s; bool bl; int a,b,c,d,e,f;
    };
    test_comparable_struct<local_comparable_struct>();

    struct local_comparable_struct_with_union {
        int i; short s; bool bl; int a,b,c,d,e; test_union u;
    };
    test_comparable_struct<local_comparable_struct_with_union>();
    
    // Making sure that test_union overloaded operations were called.
    BOOST_TEST_EQ(test_union_counter, 17);

    test_empty_struct();
    test_implicit_conversions();

    return boost::report_errors();
}


