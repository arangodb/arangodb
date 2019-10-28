
// Copyright 2016 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/equivalent.hpp"
#include "../helpers/helpers.hpp"
#include "../helpers/invariants.hpp"
#include "../helpers/random_values.hpp"
#include "../helpers/test.hpp"
#include "../helpers/tracker.hpp"
#include "../objects/test.hpp"

namespace extract_tests {

  test::seed_t initialize_seed(85638);

  template <class Container>
  void extract_tests1(Container*, test::random_generator generator)
  {
    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Extract by key.\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      int iterations = 0;
      for (typename test::random_values<Container>::iterator it = v.begin();
           it != v.end(); ++it) {
        std::size_t count = x.count(test::get_key<Container>(*it));
        std::size_t old_size = x.size();
        std::size_t new_count = count ? count - 1 : count;
        std::size_t new_size = count ? old_size - 1 : old_size;
        typename Container::node_type n =
          x.extract(test::get_key<Container>(*it));
        BOOST_TEST((n ? true : false) == (count ? true : false));
        BOOST_TEST(x.size() == new_size);
        BOOST_TEST(x.count(test::get_key<Container>(*it)) == new_count);
        if (!new_count) {
          BOOST_TEST(x.find(test::get_key<Container>(*it)) == x.end());
        } else {
          BOOST_TEST(x.find(test::get_key<Container>(*it)) != x.end());
        }
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
      BOOST_TEST(x.empty());
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "extract(begin()).\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      std::size_t size = x.size();
      int iterations = 0;
      while (size > 0 && !x.empty()) {
        typename Container::key_type key = test::get_key<Container>(*x.begin());
        std::size_t count = x.count(key);
        typename Container::node_type n = x.extract(x.begin());
        BOOST_TEST(n);
        --size;
        BOOST_TEST(x.count(key) == count - 1);
        BOOST_TEST(x.size() == size);
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
      BOOST_TEST(x.empty());
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "extract(random position).\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      std::size_t size = x.size();
      int iterations = 0;
      while (size > 0 && !x.empty()) {
        using namespace std;
        int index = rand() % (int)x.size();
        typename Container::const_iterator prev, pos, next;
        if (index == 0) {
          prev = pos = x.begin();
        } else {
          prev = test::next(x.begin(), index - 1);
          pos = test::next(prev);
        }
        next = test::next(pos);
        typename Container::key_type key = test::get_key<Container>(*pos);
        std::size_t count = x.count(key);
        typename Container::node_type n = x.extract(pos);
        BOOST_TEST(n);
        --size;
        if (size > 0)
          BOOST_TEST(index == 0 ? next == x.begin() : next == test::next(prev));
        BOOST_TEST(x.count(key) == count - 1);
        BOOST_TEST(x.size() == size);
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
      BOOST_TEST(x.empty());
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "\n";
  }

  boost::unordered_set<test::object, test::hash, test::equal_to,
    test::allocator1<test::object> >* test_set;
  boost::unordered_multiset<test::object, test::hash, test::equal_to,
    test::allocator2<test::object> >* test_multiset;
  boost::unordered_map<test::object, test::object, test::hash, test::equal_to,
    test::allocator1<test::object> >* test_map;
  boost::unordered_multimap<test::object, test::object, test::hash,
    test::equal_to, test::allocator2<test::object> >* test_multimap;

  using test::default_generator;
  using test::generate_collisions;

  UNORDERED_TEST(
    extract_tests1, ((test_set)(test_multiset)(test_map)(test_multimap))(
                      (default_generator)(generate_collisions)))
}

RUN_TESTS()
