
// Copyright 2018 Daniel James
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "./config.hpp"

#ifndef BOOST_HASH_TEST_STD_INCLUDES
#  include <boost/container_hash/hash.hpp>
#endif
#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>

#if BOOST_HASH_HAS_VARIANT

#include <variant>
#include <string>

void test_monostate()
{
    std::monostate x1;
    std::monostate x2;

    boost::hash<std::monostate> hasher;

    BOOST_TEST(hasher(x1) == hasher(x2));
}

void test_variant_int()
{
    std::variant<std::monostate, int> x1a;
    std::variant<std::monostate, int> x1b;
    std::variant<std::monostate, int> x2a(10);
    std::variant<std::monostate, int> x2b(x2a);
    std::variant<std::monostate, int> x3(20);

    boost::hash<std::variant<std::monostate, int> > hasher;

    BOOST_TEST(hasher(x1a) == hasher(x1a));
    BOOST_TEST(hasher(x1a) == hasher(x1b));
    BOOST_TEST(hasher(x1a) != hasher(x2a));
    BOOST_TEST(hasher(x1a) != hasher(x3));
    BOOST_TEST(hasher(x2a) == hasher(x2a));
    BOOST_TEST(hasher(x2b) == hasher(x2b));
    BOOST_TEST(hasher(x2a) != hasher(x3));
    BOOST_TEST(hasher(x3) == hasher(x3));
}

struct custom1 {
    int value;
    friend std::size_t hash_value(custom1 v) { return boost::hash_value(v.value); }
};

struct custom2 {
    int value;
    friend std::size_t hash_value(custom2 v) { return boost::hash_value(v.value); }
};

void test_variant_unique_types()
{
    custom1 x11 = { 0 };
    custom1 x12 = { 1 };
    custom2 x21 = { 0 };
    custom2 x22 = { 1 };

    boost::hash<custom1> hasher1;
    boost::hash<custom2> hasher2;

    BOOST_TEST(hasher1(x11) == hasher2(x21));
    BOOST_TEST(hasher1(x11) != hasher2(x22));
    BOOST_TEST(hasher1(x12) != hasher2(x21));
    BOOST_TEST(hasher1(x12) == hasher2(x22));

    typedef std::variant<custom1, custom2> variant_type;

    variant_type y11(x11);
    variant_type y12(x12);
    variant_type y21(x21);
    variant_type y22(x22);

    boost::hash<variant_type> hasher;

    BOOST_TEST(hasher(y11) != hasher(y21));
    BOOST_TEST(hasher(y11) != hasher(y22));
    BOOST_TEST(hasher(y12) != hasher(y21));
    BOOST_TEST(hasher(y12) != hasher(y22));
}

#endif

int main()
{
#if BOOST_HASH_HAS_VARIANT
    test_variant_int();
    test_variant_unique_types();
#else
    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "<variant> not available." << std::endl;
#endif
    return boost::report_errors();
}
