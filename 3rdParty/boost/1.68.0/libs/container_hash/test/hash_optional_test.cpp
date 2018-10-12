
// Copyright 2018 Daniel James
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "./config.hpp"

#ifndef BOOST_HASH_TEST_STD_INCLUDES
#  include <boost/container_hash/hash.hpp>
#endif
#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>

#if BOOST_HASH_HAS_OPTIONAL

#include <optional>
#include <string>

void test_optional_int()
{
    std::optional<int> x1a;
    std::optional<int> x1b;
    std::optional<int> x2a(10);
    std::optional<int> x2b(x2a);
    std::optional<int> x3(20);

    boost::hash<std::optional<int> > hasher;

    BOOST_TEST(hasher(x1a) == hasher(x1a));
    BOOST_TEST(hasher(x1a) == hasher(x1b));
    BOOST_TEST(hasher(x1a) != hasher(x2a));
    BOOST_TEST(hasher(x1a) != hasher(x3));
    BOOST_TEST(hasher(x2a) == hasher(x2a));
    BOOST_TEST(hasher(x2b) == hasher(x2b));
    BOOST_TEST(hasher(x2a) != hasher(x3));
    BOOST_TEST(hasher(x3) == hasher(x3));
}

void test_optional_string()
{
    std::optional<std::string> x1a;
    std::optional<std::string> x1b;
    std::optional<std::string> x2a("10");
    std::optional<std::string> x2b(x2a);
    std::optional<std::string> x3("20");

    boost::hash<std::optional<std::string> > hasher;

    BOOST_TEST(hasher(x1a) == hasher(x1a));
    BOOST_TEST(hasher(x1a) == hasher(x1b));
    BOOST_TEST(hasher(x1a) != hasher(x2a));
    BOOST_TEST(hasher(x1a) != hasher(x3));
    BOOST_TEST(hasher(x2a) == hasher(x2a));
    BOOST_TEST(hasher(x2b) == hasher(x2b));
    BOOST_TEST(hasher(x2a) != hasher(x3));
    BOOST_TEST(hasher(x3) == hasher(x3));
}

#endif

int main()
{
#if BOOST_HASH_HAS_OPTIONAL
    test_optional_int();
    test_optional_string();
#else
    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "<optional> not available." << std::endl;
#endif
    return boost::report_errors();
}
