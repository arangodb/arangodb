
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/test.hpp"
#include "../objects/test.hpp"
#include "../helpers/random_values.hpp"
#include "../helpers/tracker.hpp"
#include "../helpers/equivalent.hpp"
#include "../helpers/helpers.hpp"
#include "../helpers/invariants.hpp"
#include <vector>
#include <cstdlib>

namespace erase_tests {

  test::seed_t initialize_seed(85638);

  template <class Container>
  void erase_tests1(Container*, test::random_generator generator)
  {
    typedef typename Container::iterator iterator;
    typedef typename Container::const_iterator c_iterator;

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Erase by key.\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      int iterations = 0;
      for (typename test::random_values<Container>::iterator it = v.begin();
           it != v.end(); ++it) {
        std::size_t count = x.count(test::get_key<Container>(*it));
        std::size_t old_size = x.size();
        BOOST_TEST(count == x.erase(test::get_key<Container>(*it)));
        BOOST_TEST(x.size() == old_size - count);
        BOOST_TEST(x.count(test::get_key<Container>(*it)) == 0);
        BOOST_TEST(x.find(test::get_key<Container>(*it)) == x.end());
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "erase(begin()).\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      std::size_t size = x.size();
      int iterations = 0;
      while (size > 0 && !x.empty()) {
        typename Container::key_type key = test::get_key<Container>(*x.begin());
        std::size_t count = x.count(key);
        iterator pos = x.erase(x.begin());
        --size;
        BOOST_TEST(pos == x.begin());
        BOOST_TEST(x.count(key) == count - 1);
        BOOST_TEST(x.size() == size);
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
      BOOST_TEST(x.empty());
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "erase(random position).\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      std::size_t size = x.size();
      int iterations = 0;
      while (size > 0 && !x.empty()) {
        std::size_t index = test::random_value(x.size());
        c_iterator prev, pos, next;
        if (index == 0) {
          prev = pos = x.begin();
        } else {
          prev = test::next(x.begin(), index - 1);
          pos = test::next(prev);
        }
        next = test::next(pos);
        typename Container::key_type key = test::get_key<Container>(*pos);
        std::size_t count = x.count(key);
        BOOST_TEST(count > 0);
        BOOST_TEST(next == x.erase(pos));
        --size;
        if (size > 0)
          BOOST_TEST(index == 0 ? next == x.begin() : next == test::next(prev));
        BOOST_TEST(x.count(key) == count - 1);
        if (x.count(key) != count - 1) {
          BOOST_LIGHTWEIGHT_TEST_OSTREAM << count << " => " << x.count(key)
                                         << std::endl;
        }
        BOOST_TEST(x.size() == size);
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
      BOOST_TEST(x.empty());
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "erase(ranges).\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(500, generator);
      Container x(v.begin(), v.end());

      std::size_t size = x.size();

      // I'm actually stretching it a little here, as the standard says it
      // returns 'the iterator immediately following the erase elements'
      // and if nothing is erased, then there's nothing to follow. But I
      // think this is the only sensible option...
      BOOST_TEST(x.erase(x.end(), x.end()) == x.end());
      BOOST_TEST(x.erase(x.begin(), x.begin()) == x.begin());
      BOOST_TEST(x.size() == size);
      test::check_equivalent_keys(x);

      BOOST_TEST(x.erase(x.begin(), x.end()) == x.end());
      BOOST_TEST(x.empty());
      BOOST_TEST(x.begin() == x.end());
      test::check_equivalent_keys(x);

      BOOST_TEST(x.erase(x.begin(), x.end()) == x.begin());
      test::check_equivalent_keys(x);
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "erase(random ranges).\n";
    {
      test::check_instances check_;
      Container x;

      for (int i = 0; i < 100; ++i) {
        test::random_values<Container> v(1000, generator);
        x.insert(v.begin(), v.end());

        // Note that erase only invalidates the erased iterators.
        std::vector<c_iterator> iterators;
        for (c_iterator it = x.cbegin(); it != x.cend(); ++it) {
          iterators.push_back(it);
        }
        iterators.push_back(x.cend());

        while (iterators.size() > 1) {
          std::size_t start = test::random_value(iterators.size());
          std::size_t length = test::random_value(iterators.size() - start);
          x.erase(iterators[start], iterators[start + length]);
          iterators.erase(test::next(iterators.begin(), start),
            test::next(iterators.begin(), start + length));

          BOOST_TEST(x.size() == iterators.size() - 1);
          typename std::vector<c_iterator>::const_iterator i2 =
            iterators.begin();
          for (c_iterator i1 = x.cbegin(); i1 != x.cend(); ++i1) {
            BOOST_TEST(i1 == *i2);
            ++i2;
          }
          BOOST_TEST(x.cend() == *i2);

          test::check_equivalent_keys(x);
        }
        BOOST_TEST(x.empty());
      }
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "quick_erase(begin()).\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      std::size_t size = x.size();
      int iterations = 0;
      while (size > 0 && !x.empty()) {
        typename Container::key_type key = test::get_key<Container>(*x.begin());
        std::size_t count = x.count(key);
        x.quick_erase(x.begin());
        --size;
        BOOST_TEST(x.count(key) == count - 1);
        BOOST_TEST(x.size() == size);
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
      BOOST_TEST(x.empty());
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "quick_erase(random position).\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(1000, generator);
      Container x(v.begin(), v.end());
      std::size_t size = x.size();
      int iterations = 0;
      while (size > 0 && !x.empty()) {
        std::size_t index = test::random_value(x.size());
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
        BOOST_TEST(count > 0);
        x.quick_erase(pos);
        --size;
        if (size > 0)
          BOOST_TEST(index == 0 ? next == x.begin() : next == test::next(prev));
        BOOST_TEST(x.count(key) == count - 1);
        if (x.count(key) != count - 1) {
          BOOST_LIGHTWEIGHT_TEST_OSTREAM << count << " => " << x.count(key)
                                         << std::endl;
        }
        BOOST_TEST(x.size() == size);
        if (++iterations % 20 == 0)
          test::check_equivalent_keys(x);
      }
      BOOST_TEST(x.empty());
    }

    BOOST_LIGHTWEIGHT_TEST_OSTREAM << "clear().\n";
    {
      test::check_instances check_;

      test::random_values<Container> v(500, generator);
      Container x(v.begin(), v.end());
      x.clear();
      BOOST_TEST(x.empty());
      BOOST_TEST(x.begin() == x.end());
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
  using test::limited_range;

  UNORDERED_TEST(
    erase_tests1, ((test_set)(test_multiset)(test_map)(test_multimap))(
                    (default_generator)(generate_collisions)(limited_range)))
}

RUN_TESTS()
