
// Copyright 2016 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/test.hpp"
#include "../helpers/invariants.hpp"

#include <map>
#include <set>

namespace insert_hint {
  UNORDERED_AUTO_TEST (insert_hint_empty) {
    typedef boost::unordered_multiset<int> container;
    container x;
    x.insert(x.cbegin(), 10);
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST_EQ(x.count(10), 1u);
    test::check_equivalent_keys(x);
  }

  UNORDERED_AUTO_TEST (insert_hint_empty2) {
    typedef boost::unordered_multimap<std::string, int> container;
    container x;
    x.emplace_hint(x.cbegin(), "hello", 50);
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST_EQ(x.count("hello"), 1u);
    BOOST_TEST_EQ(x.find("hello")->second, 50);
    test::check_equivalent_keys(x);
  }

  UNORDERED_AUTO_TEST (insert_hint_single) {
    typedef boost::unordered_multiset<std::string> container;
    container x;
    x.insert("equal");
    x.insert(x.cbegin(), "equal");
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST_EQ(x.count("equal"), 2u);
    test::check_equivalent_keys(x);
  }

  UNORDERED_AUTO_TEST (insert_hint_single2) {
    typedef boost::unordered_multimap<int, std::string> container;
    container x;
    x.emplace(10, "one");
    x.emplace_hint(x.cbegin(), 10, "two");
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST_EQ(x.count(10), 2u);

    container::iterator it = x.find(10);
    std::string v0 = (it++)->second;
    std::string v1 = (it++)->second;

    BOOST_TEST(v0 == "one" || v0 == "two");
    BOOST_TEST(v1 == "one" || v1 == "two");
    BOOST_TEST(v0 != v1);

    test::check_equivalent_keys(x);
  }

  UNORDERED_AUTO_TEST (insert_hint_multiple) {
    for (unsigned int size = 0; size < 10; ++size) {
      for (unsigned int offset = 0; offset <= size; ++offset) {
        typedef boost::unordered_multiset<std::string> container;
        container x;

        for (unsigned int i = 0; i < size; ++i) {
          x.insert("multiple");
        }

        BOOST_TEST_EQ(x.size(), size);

        container::const_iterator position = x.cbegin();
        for (unsigned int i = 0; i < offset; ++i) {
          ++position;
        }

        x.insert(position, "multiple");

        BOOST_TEST_EQ(x.size(), size + 1u);
        BOOST_TEST_EQ(x.count("multiple"), size + 1u);
        test::check_equivalent_keys(x);
      }
    }
  }

  UNORDERED_AUTO_TEST (insert_hint_unique) {
    typedef boost::unordered_set<int> container;
    container x;
    x.insert(x.cbegin(), 10);
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST_EQ(x.count(10), 1u);
    test::check_equivalent_keys(x);
  }

  UNORDERED_AUTO_TEST (insert_hint_unique_single) {
    typedef boost::unordered_set<int> container;
    container x;
    x.insert(10);

    x.insert(x.cbegin(), 10);
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST_EQ(x.count(10), 1u);
    test::check_equivalent_keys(x);

    x.insert(x.cbegin(), 20);
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST_EQ(x.count(10), 1u);
    BOOST_TEST_EQ(x.count(20), 1u);
    test::check_equivalent_keys(x);
  }
}

RUN_TESTS()
