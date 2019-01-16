
// Copyright 2017-2018 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../helpers/exception_test.hpp"
#include "../helpers/invariants.hpp"
#include "../helpers/metafunctions.hpp"
#include "../helpers/random_values.hpp"
#include "./containers.hpp"

template <typename T1, typename T2> void merge_exception_test(T1 x, T2 y)
{
  std::size_t size = x.size() + y.size();

  try {
    ENABLE_EXCEPTIONS;
    x.merge(y);
  } catch (...) {
    test::check_equivalent_keys(x);
    test::check_equivalent_keys(y);
    throw;
  }

  // Not a full check, just want to make sure the merge completed.
  BOOST_TEST(size == x.size() + y.size());
  if (y.size()) {
    BOOST_TEST(test::has_unique_keys<T1>::value);
    for (typename T2::iterator it = y.begin(); it != y.end(); ++it) {
      BOOST_TEST(x.find(test::get_key<T2>(*it)) != x.end());
    }
  }
  test::check_equivalent_keys(x);
  test::check_equivalent_keys(y);
}

template <typename T1, typename T2>
void merge_exception_test(T1 const*, T2 const*, std::size_t count12, int tag12,
  test::random_generator gen1, test::random_generator gen2)
{
  std::size_t count1 = count12 / 256;
  std::size_t count2 = count12 % 256;
  int tag1 = tag12 / 256;
  int tag2 = tag12 % 256;
  test::random_values<T1> v1(count1, gen1);
  test::random_values<T2> v2(count2, gen2);
  T1 x(v1.begin(), v1.end(), 0, test::exception::hash(tag1),
    test::exception::equal_to(tag1));
  T2 y(v2.begin(), v2.end(), 0, test::exception::hash(tag2),
    test::exception::equal_to(tag2));

  EXCEPTION_LOOP(merge_exception_test(x, y))
}

boost::unordered_set<test::exception::object, test::exception::hash,
  test::exception::equal_to,
  test::exception::allocator<test::exception::object> >* test_set_;
boost::unordered_multiset<test::exception::object, test::exception::hash,
  test::exception::equal_to,
  test::exception::allocator<test::exception::object> >* test_multiset_;
boost::unordered_map<test::exception::object, test::exception::object,
  test::exception::hash, test::exception::equal_to,
  test::exception::allocator2<test::exception::object> >* test_map_;
boost::unordered_multimap<test::exception::object, test::exception::object,
  test::exception::hash, test::exception::equal_to,
  test::exception::allocator2<test::exception::object> >* test_multimap_;

using test::default_generator;
using test::generate_collisions;
using test::limited_range;

// clang-format off
UNORDERED_MULTI_TEST(set_merge, merge_exception_test,
    ((test_set_)(test_multiset_))
    ((test_set_)(test_multiset_))
    ((0x0000)(0x6400)(0x0064)(0x0a64)(0x3232))
    ((0x0000)(0x0001)(0x0102))
    ((default_generator)(limited_range))
    ((default_generator)(limited_range))
)
UNORDERED_MULTI_TEST(map_merge, merge_exception_test,
    ((test_map_)(test_multimap_))
    ((test_map_)(test_multimap_))
    ((0x0000)(0x6400)(0x0064)(0x0a64)(0x3232))
    ((0x0101)(0x0200)(0x0201))
    ((default_generator)(limited_range))
    ((default_generator)(limited_range))
)
// Run fewer generate_collisions tests, as they're slow.
UNORDERED_MULTI_TEST(set_merge_collisions, merge_exception_test,
    ((test_set_)(test_multiset_))
    ((test_set_)(test_multiset_))
    ((0x0a0a))
    ((0x0202)(0x0100)(0x0201))
    ((generate_collisions))
    ((generate_collisions))
)
UNORDERED_MULTI_TEST(map_merge_collisions, merge_exception_test,
    ((test_map_)(test_multimap_))
    ((test_map_)(test_multimap_))
    ((0x0a0a))
    ((0x0000)(0x0002)(0x0102))
    ((generate_collisions))
    ((generate_collisions))
)
// clang-format on

RUN_TESTS_QUIET()
