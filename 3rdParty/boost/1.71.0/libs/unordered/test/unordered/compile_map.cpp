
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This test creates the containers with members that meet their minimum
// requirements. Makes sure everything compiles and is defined correctly.

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_map.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/test.hpp"
#include "../objects/minimal.hpp"
#include "./compile_tests.hpp"

// Explicit instantiation to catch compile-time errors

#define INSTANTIATE(type)                                                      \
  template class boost::unordered::detail::instantiate_##type

INSTANTIATE(map)<int, int, boost::hash<int>, std::equal_to<int>,
  test::minimal::allocator<int> >;
INSTANTIATE(multimap)<int const, int const, boost::hash<int>,
  std::equal_to<int>, test::minimal::allocator<int> >;

INSTANTIATE(
  map)<test::minimal::assignable const, test::minimal::default_assignable const,
  test::minimal::hash<test::minimal::assignable>,
  test::minimal::equal_to<test::minimal::assignable>,
  test::minimal::allocator<int> >;
INSTANTIATE(multimap)<test::minimal::assignable, test::minimal::assignable,
  test::minimal::hash<test::minimal::assignable>,
  test::minimal::equal_to<test::minimal::assignable>,
  test::minimal::allocator<int> >;

UNORDERED_AUTO_TEST (test0) {
  test::minimal::constructor_param x;

  typedef std::pair<test::minimal::assignable const, test::minimal::assignable>
    value_type;
  value_type value(x, x);

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Test unordered_map.\n";

  boost::unordered_map<int, int> int_map;

  boost::unordered_map<int, int, boost::hash<int>, std::equal_to<int>,
    test::minimal::cxx11_allocator<std::pair<int const, int> > >
    int_map2;

  boost::unordered_map<test::minimal::assignable, test::minimal::assignable,
    test::minimal::hash<test::minimal::assignable>,
    test::minimal::equal_to<test::minimal::assignable>,
    test::minimal::allocator<value_type> >
    map;

  container_test(int_map, std::pair<int const, int>(0, 0));
  container_test(int_map2, std::pair<int const, int>(0, 0));
  container_test(map, value);

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Test unordered_multimap.\n";

  boost::unordered_multimap<int, int> int_multimap;

  boost::unordered_multimap<int, int, boost::hash<int>, std::equal_to<int>,
    test::minimal::cxx11_allocator<std::pair<int const, int> > >
    int_multimap2;

  boost::unordered_multimap<test::minimal::assignable,
    test::minimal::assignable, test::minimal::hash<test::minimal::assignable>,
    test::minimal::equal_to<test::minimal::assignable>,
    test::minimal::allocator<value_type> >
    multimap;

  container_test(int_multimap, std::pair<int const, int>(0, 0));
  container_test(int_multimap2, std::pair<int const, int>(0, 0));
  container_test(multimap, value);
}

UNORDERED_AUTO_TEST (equality_tests) {
  typedef std::pair<test::minimal::copy_constructible_equality_comparable const,
    test::minimal::copy_constructible_equality_comparable>
    value_type;

  boost::unordered_map<int, int> int_map;

  boost::unordered_map<int, int, boost::hash<int>, std::equal_to<int>,
    test::minimal::cxx11_allocator<std::pair<int const, int> > >
    int_map2;

  boost::unordered_map<test::minimal::copy_constructible_equality_comparable,
    test::minimal::copy_constructible_equality_comparable,
    test::minimal::hash<test::minimal::copy_constructible_equality_comparable>,
    test::minimal::equal_to<
      test::minimal::copy_constructible_equality_comparable>,
    test::minimal::allocator<value_type> >
    map;

  equality_test(int_map);
  equality_test(int_map2);
  equality_test(map);

  boost::unordered_multimap<int, int> int_multimap;

  boost::unordered_multimap<int, int, boost::hash<int>, std::equal_to<int>,
    test::minimal::cxx11_allocator<std::pair<int const, int> > >
    int_multimap2;

  boost::unordered_multimap<
    test::minimal::copy_constructible_equality_comparable,
    test::minimal::copy_constructible_equality_comparable,
    test::minimal::hash<test::minimal::copy_constructible_equality_comparable>,
    test::minimal::equal_to<
      test::minimal::copy_constructible_equality_comparable>,
    test::minimal::allocator<value_type> >
    multimap;

  equality_test(int_multimap);
  equality_test(int_multimap2);
  equality_test(multimap);
}

UNORDERED_AUTO_TEST (test1) {
  boost::hash<int> hash;
  std::equal_to<int> equal_to;
  int value = 0;
  std::pair<int const, int> map_value(0, 0);

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Test unordered_map.\n";

  boost::unordered_map<int, int> map;

  boost::unordered_map<int, int, boost::hash<int>, std::equal_to<int>,
    test::minimal::cxx11_allocator<std::pair<int const, int> > >
    map2;

  unordered_unique_test(map, map_value);
  unordered_map_test(map, value, value);
  unordered_copyable_test(map, value, map_value, hash, equal_to);
  unordered_map_functions(map, value, value);

  unordered_unique_test(map2, map_value);
  unordered_map_test(map2, value, value);
  unordered_copyable_test(map2, value, map_value, hash, equal_to);
  unordered_map_functions(map2, value, value);

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Test unordered_multimap.\n";

  boost::unordered_multimap<int, int> multimap;

  boost::unordered_multimap<int, int, boost::hash<int>, std::equal_to<int>,
    test::minimal::cxx11_allocator<std::pair<int const, int> > >
    multimap2;

  unordered_equivalent_test(multimap, map_value);
  unordered_map_test(multimap, value, value);
  unordered_copyable_test(multimap, value, map_value, hash, equal_to);

  unordered_equivalent_test(multimap2, map_value);
  unordered_map_test(multimap2, value, value);
  unordered_copyable_test(multimap2, value, map_value, hash, equal_to);
}

UNORDERED_AUTO_TEST (test2) {
  test::minimal::constructor_param x;

  test::minimal::assignable assignable(x);
  test::minimal::copy_constructible copy_constructible(x);
  test::minimal::hash<test::minimal::assignable> hash(x);
  test::minimal::equal_to<test::minimal::assignable> equal_to(x);

  typedef std::pair<test::minimal::assignable const, test::minimal::assignable>
    map_value_type;
  map_value_type map_value(assignable, assignable);

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Test unordered_map.\n";

  boost::unordered_map<test::minimal::assignable, test::minimal::assignable,
    test::minimal::hash<test::minimal::assignable>,
    test::minimal::equal_to<test::minimal::assignable>,
    test::minimal::allocator<map_value_type> >
    map;

  unordered_unique_test(map, map_value);
  unordered_map_test(map, assignable, assignable);
  unordered_copyable_test(map, assignable, map_value, hash, equal_to);
  unordered_map_member_test(map, map_value);

  boost::unordered_map<test::minimal::assignable,
    test::minimal::default_assignable,
    test::minimal::hash<test::minimal::assignable>,
    test::minimal::equal_to<test::minimal::assignable>,
    test::minimal::allocator<map_value_type> >
    map2;

  test::minimal::default_assignable default_assignable;

  unordered_map_functions(map2, assignable, default_assignable);

  BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Test unordered_multimap.\n";

  boost::unordered_multimap<test::minimal::assignable,
    test::minimal::assignable, test::minimal::hash<test::minimal::assignable>,
    test::minimal::equal_to<test::minimal::assignable>,
    test::minimal::allocator<map_value_type> >
    multimap;

  unordered_equivalent_test(multimap, map_value);
  unordered_map_test(multimap, assignable, assignable);
  unordered_copyable_test(multimap, assignable, map_value, hash, equal_to);
  unordered_map_member_test(multimap, map_value);
}

// Test for ambiguity when using key convertible from iterator
// See LWG2059

struct lwg2059_key
{
  int value;

  template <typename T> lwg2059_key(T v) : value(v) {}
};

std::size_t hash_value(lwg2059_key x)
{
  return static_cast<std::size_t>(x.value);
}

bool operator==(lwg2059_key x, lwg2059_key y) { return x.value == y.value; }

UNORDERED_AUTO_TEST (lwg2059) {
  {
    boost::unordered_map<lwg2059_key, int> x;
    x.emplace(lwg2059_key(10), 5);
    x.erase(x.begin());
  }

  {
    boost::unordered_multimap<lwg2059_key, int> x;
    x.emplace(lwg2059_key(10), 5);
    x.erase(x.begin());
  }
}

RUN_TESTS()
