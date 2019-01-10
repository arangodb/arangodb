
// Copyright 2016 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../helpers/postfix.hpp"
#include "../helpers/prefix.hpp"
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "../helpers/count.hpp"
#include "../helpers/helpers.hpp"
#include "../helpers/invariants.hpp"
#include "../helpers/random_values.hpp"
#include "../helpers/test.hpp"
#include "../helpers/tracker.hpp"
#include "../objects/test.hpp"

namespace merge_tests {

  UNORDERED_AUTO_TEST (merge_set) {
    boost::unordered_set<int> x;
    boost::unordered_set<int> y;

    x.merge(y);
    BOOST_TEST(x.empty());
    BOOST_TEST(y.empty());

    x.insert(10);
    x.merge(y);
    BOOST_TEST(x.size() == 1);
    BOOST_TEST(x.count(10) == 1);
    BOOST_TEST(y.empty());

    y.merge(x);
    BOOST_TEST(x.empty());
    BOOST_TEST(y.size() == 1);
    BOOST_TEST(y.count(10) == 1);

    x.insert(10);
    x.insert(50);
    y.insert(70);
    y.insert(80);
    x.merge(y);
    BOOST_TEST_EQ(x.size(), 4u);
    BOOST_TEST_EQ(y.size(), 1u);
    BOOST_TEST_EQ(x.count(10), 1u);
    BOOST_TEST_EQ(x.count(50), 1u);
    BOOST_TEST_EQ(x.count(70), 1u);
    BOOST_TEST_EQ(x.count(80), 1u);
    BOOST_TEST_EQ(y.count(10), 1u);
    BOOST_TEST_EQ(y.count(50), 0u);
    BOOST_TEST_EQ(y.count(70), 0u);
    BOOST_TEST_EQ(y.count(80), 0u);

    test::check_equivalent_keys(x);
    test::check_equivalent_keys(y);
  }

  UNORDERED_AUTO_TEST (merge_multiset) {
    boost::unordered_multiset<int> x;
    boost::unordered_multiset<int> y;

    x.merge(y);
    BOOST_TEST(x.empty());
    BOOST_TEST(y.empty());

    x.insert(10);
    x.merge(y);
    BOOST_TEST(x.size() == 1);
    BOOST_TEST(x.count(10) == 1);
    BOOST_TEST(y.empty());

    y.merge(x);
    BOOST_TEST(x.empty());
    BOOST_TEST(y.size() == 1);
    BOOST_TEST(y.count(10) == 1);

    x.insert(10);
    x.insert(50);
    y.insert(70);
    y.insert(80);
    x.merge(y);
    BOOST_TEST_EQ(x.size(), 5u);
    BOOST_TEST_EQ(y.size(), 0u);
    BOOST_TEST_EQ(x.count(10), 2u);
    BOOST_TEST_EQ(x.count(50), 1u);
    BOOST_TEST_EQ(x.count(70), 1u);
    BOOST_TEST_EQ(x.count(80), 1u);
    BOOST_TEST_EQ(y.count(10), 0u);
    BOOST_TEST_EQ(y.count(50), 0u);
    BOOST_TEST_EQ(y.count(70), 0u);
    BOOST_TEST_EQ(y.count(80), 0u);

    test::check_equivalent_keys(x);
    test::check_equivalent_keys(y);
  }

  UNORDERED_AUTO_TEST (merge_set_and_multiset) {
    boost::unordered_set<int> x;
    boost::unordered_multiset<int> y;

    x.merge(y);
    BOOST_TEST(x.empty());
    BOOST_TEST(y.empty());

    x.insert(10);
    x.merge(y);
    BOOST_TEST(x.size() == 1);
    BOOST_TEST(x.count(10) == 1);
    BOOST_TEST(y.empty());

    y.merge(x);
    BOOST_TEST(x.empty());
    BOOST_TEST(y.size() == 1);
    BOOST_TEST(y.count(10) == 1);

    x.insert(10);
    x.insert(50);
    y.insert(70);
    y.insert(80);
    x.merge(y);
    BOOST_TEST_EQ(x.size(), 4u);
    BOOST_TEST_EQ(y.size(), 1u);
    BOOST_TEST_EQ(x.count(10), 1u);
    BOOST_TEST_EQ(x.count(50), 1u);
    BOOST_TEST_EQ(x.count(70), 1u);
    BOOST_TEST_EQ(x.count(80), 1u);
    BOOST_TEST_EQ(y.count(10), 1u);
    BOOST_TEST_EQ(y.count(50), 0u);
    BOOST_TEST_EQ(y.count(70), 0u);
    BOOST_TEST_EQ(y.count(80), 0u);

    test::check_equivalent_keys(x);
    test::check_equivalent_keys(y);
  }

  template <class X1, class X2>
  void merge_empty_test(X1*, X2*, test::random_generator generator)
  {
    test::check_instances check_;

    test::random_values<X1> v(1000, generator);
    X1 x1(v.begin(), v.end());
    X2 x2;
    x1.merge(x2);
    test::check_container(x1, v);
    BOOST_TEST(x2.empty());
    test::check_equivalent_keys(x1);
    test::check_equivalent_keys(x2);
  }

  template <class X>
  void merge_into_empty_test(X*, test::random_generator generator)
  {
    test::check_instances check_;

    test::random_values<X> v(1000, generator);
    X x1;
    X x2(v.begin(), v.end());
    x1.merge(x2);
    test::check_container(x1, v);
    BOOST_TEST(x2.empty());
    test::check_equivalent_keys(x1);
    test::check_equivalent_keys(x2);
  }

  template <class X1, class X2>
  void merge_into_unique_keys_test(X1*, X2*, int hash_equal1, int hash_equal2,
    test::random_generator generator)
  {
    test::check_instances check_;

    test::random_values<X1> v1(1000, generator);
    test::random_values<X2> v2(1000, generator);
    v1.insert(v2.begin(), test::next(v2.begin(), 100));
    v2.insert(v1.begin(), test::next(v1.begin(), 100));

    X1 x1(v1.begin(), v1.end(), 0, test::hash(hash_equal1),
      test::equal_to(hash_equal1));
    X2 x2(v2.begin(), v2.end(), 0, test::hash(hash_equal2),
      test::equal_to(hash_equal2));

    test::ordered<X1> tracker1 = test::create_ordered(x1);
    test::ordered<X2> tracker2 = test::create_ordered(x2);
    tracker1.insert(v1.begin(), v1.end());
    for (typename X2::iterator it = x2.begin(); it != x2.end(); ++it) {
      if (!tracker1.insert(*it).second) {
        tracker2.insert(*it);
      }
    }

    x1.merge(x2);

    tracker1.compare(x1);
    tracker2.compare(x2);
    test::check_equivalent_keys(x1);
    test::check_equivalent_keys(x2);
  }

  template <class X1, class X2>
  void merge_into_equiv_keys_test(X1*, X2*, int hash_equal1, int hash_equal2,
    test::random_generator generator)
  {
    test::check_instances check_;

    test::random_values<X1> v1(1000, generator);
    test::random_values<X2> v2(1000, generator);
    v1.insert(v2.begin(), test::next(v2.begin(), 100));
    v2.insert(v1.begin(), test::next(v1.begin(), 100));

    X1 x1(v1.begin(), v1.end(), 0, test::hash(hash_equal1),
      test::equal_to(hash_equal1));
    X2 x2(v2.begin(), v2.end(), 0, test::hash(hash_equal2),
      test::equal_to(hash_equal2));
    x1.merge(x2);

    test::ordered<X1> tracker1 = test::create_ordered(x1);
    test::ordered<X2> tracker2 = test::create_ordered(x2);
    tracker1.insert(v1.begin(), v1.end());
    tracker2.insert(v2.begin(), v2.end());
    tracker1.insert(tracker2.begin(), tracker2.end());
    tracker2.clear();

    tracker1.compare(x1);
    tracker2.compare(x2);
    test::check_equivalent_keys(x1);
    test::check_equivalent_keys(x2);
  }

  boost::unordered_set<test::movable, test::hash, test::equal_to,
    std::allocator<test::movable> >* test_set_std_alloc;
  boost::unordered_multiset<test::movable, test::hash, test::equal_to,
    std::allocator<test::movable> >* test_multiset_std_alloc;

  boost::unordered_map<test::object, test::object, test::hash, test::equal_to,
    std::allocator<test::object> >* test_map_std_alloc;
  boost::unordered_multimap<test::object, test::object, test::hash,
    test::equal_to, std::allocator<test::object> >* test_multimap_std_alloc;

  boost::unordered_set<test::object, test::hash, test::equal_to,
    test::allocator1<test::object> >* test_set;
  boost::unordered_multiset<test::object, test::hash, test::equal_to,
    test::allocator1<test::object> >* test_multiset;

  boost::unordered_map<test::movable, test::movable, test::hash, test::equal_to,
    test::allocator2<test::movable> >* test_map;
  boost::unordered_multimap<test::movable, test::movable, test::hash,
    test::equal_to, test::allocator2<test::movable> >* test_multimap;

  using test::default_generator;
  using test::generate_collisions;

  // clang-format off
UNORDERED_TEST(merge_empty_test,
    ((test_set_std_alloc)(test_multiset_std_alloc))
    ((test_set_std_alloc)(test_multiset_std_alloc))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_empty_test,
    ((test_map_std_alloc)(test_multimap_std_alloc))
    ((test_map_std_alloc)(test_multimap_std_alloc))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_empty_test,
    ((test_set)(test_multiset))
    ((test_set)(test_multiset))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_empty_test,
    ((test_map)(test_multimap))
    ((test_map)(test_multimap))
    ((default_generator)(generate_collisions)))

UNORDERED_TEST(merge_into_empty_test,
    ((test_set_std_alloc)(test_multiset_std_alloc))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_empty_test,
    ((test_map_std_alloc)(test_multimap_std_alloc))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_empty_test,
    ((test_set)(test_multiset))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_empty_test,
    ((test_map)(test_multimap))
    ((default_generator)(generate_collisions)))

UNORDERED_TEST(merge_into_unique_keys_test,
    ((test_set_std_alloc))
    ((test_set_std_alloc)(test_multiset_std_alloc))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_unique_keys_test,
    ((test_map_std_alloc))
    ((test_map_std_alloc)(test_multimap_std_alloc))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_unique_keys_test,
    ((test_set))
    ((test_set)(test_multiset))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_unique_keys_test,
    ((test_map))
    ((test_map)(test_multimap))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))

UNORDERED_TEST(merge_into_equiv_keys_test,
    ((test_multiset_std_alloc))
    ((test_set_std_alloc)(test_multiset_std_alloc))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_equiv_keys_test,
    ((test_multimap_std_alloc))
    ((test_map_std_alloc)(test_multimap_std_alloc))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_equiv_keys_test,
    ((test_multiset))
    ((test_set)(test_multiset))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))
UNORDERED_TEST(merge_into_equiv_keys_test,
    ((test_multimap))
    ((test_map)(test_multimap))
    ((0)(1)(2))
    ((0)(1)(2))
    ((default_generator)(generate_collisions)))
  // clang-format on
}

RUN_TESTS()
