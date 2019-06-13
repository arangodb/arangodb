
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#include "./containers.hpp"

#include "../helpers/helpers.hpp"
#include "../helpers/invariants.hpp"
#include "../helpers/random_values.hpp"
#include "../helpers/strong.hpp"
#include "../helpers/tracker.hpp"
#include <cmath>
#include <string>

test::seed_t initialize_seed(747373);

// Fill in a container so that it's about to rehash
template <typename T> void rehash_prep(T& x)
{
  using namespace std;
  typedef typename T::size_type size_type;

  x.max_load_factor(0.25);
  size_type bucket_count = x.bucket_count();
  size_type initial_elements = static_cast<size_type>(
    ceil((double)bucket_count * (double)x.max_load_factor()) - 1);
  test::random_values<T> v(initial_elements);
  x.insert(v.begin(), v.end());
  BOOST_TEST(bucket_count == x.bucket_count());
}

// Overload to generate inserters that need type information.

template <typename Inserter, typename T>
Inserter generate(Inserter inserter, T&)
{
  return inserter;
}

// Get the iterator returned from an insert/emplace.

template <typename T> T get_iterator(T const& x) { return x; }

template <typename T> T get_iterator(std::pair<T, bool> const& x)
{
  return x.first;
}

// Generic insert exception test for typical single element inserts..

template <typename T, typename Inserter, typename Values>
void insert_exception_test_impl(T x, Inserter insert, Values const& v)
{
  test::strong<T> strong;

  test::ordered<T> tracker;
  tracker.insert(x.begin(), x.end());

  try {
    ENABLE_EXCEPTIONS;

    for (typename Values::const_iterator it = v.begin(); it != v.end(); ++it) {
      strong.store(x, test::detail::tracker.count_allocations);
      insert(x, it);
    }
  } catch (...) {
    test::check_equivalent_keys(x);
    insert.exception_check(x, strong);
    throw;
  }

  test::check_equivalent_keys(x);
  insert.track(tracker, v.begin(), v.end());
  tracker.compare(x);
}

// Simple insert exception test

template <typename T, typename Inserter>
void insert_exception_test(T*, Inserter insert, test::random_generator gen)
{
  for (int i = 0; i < 5; ++i) {
    test::random_values<T> v(10, gen);
    T x;

    EXCEPTION_LOOP(insert_exception_test_impl(x, generate(insert, x), v));
  }
}

// Insert into a container which is about to hit its max load, so that it
// rehashes.

template <typename T, typename Inserter>
void insert_rehash_exception_test(
  T*, Inserter insert, test::random_generator gen)
{
  for (int i = 0; i < 5; ++i) {
    T x;
    rehash_prep(x);

    test::random_values<T> v2(5, gen);
    EXCEPTION_LOOP(insert_exception_test_impl(x, generate(insert, x), v2));
  }
}

// Various methods for inserting a single element

struct inserter_base
{
  template <typename T> void exception_check(T& x, test::strong<T>& strong)
  {
    std::string scope(test::scope);

    if (scope.find("hash::operator()") == std::string::npos)
      strong.test(x, test::detail::tracker.count_allocations);
  }

  template <typename T, typename Iterator>
  void track(T& tracker, Iterator begin, Iterator end)
  {
    tracker.insert(begin, end);
  }
};

struct insert_lvalue_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.insert(*it);
  }
} insert_lvalue;

struct insert_lvalue_begin_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.insert(x.begin(), *it);
  }
} insert_lvalue_begin;

struct insert_lvalue_end_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.insert(x.end(), *it);
  }
} insert_lvalue_end;

template <typename T> struct insert_lvalue_pos_type_impl : inserter_base
{
  typename T::iterator pos;

  insert_lvalue_pos_type_impl(T& x) : pos(x.begin()) {}

  template <typename Iterator> void operator()(T& x, Iterator it)
  {
    pos = get_iterator(x.insert(pos, *it));
  }
};

struct insert_lvalue_pos_type
{
  template <typename T>
  friend insert_lvalue_pos_type_impl<T> generate(insert_lvalue_pos_type, T& x)
  {
    return insert_lvalue_pos_type_impl<T>(x);
  }
} insert_lvalue_pos;

struct insert_single_item_range_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.insert(it, test::next(it));
  }
} insert_single_item_range;

struct emplace_lvalue_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.emplace(*it);
  }
} emplace_lvalue;

struct emplace_lvalue_begin_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.emplace_hint(x.begin(), *it);
  }
} emplace_lvalue_begin;

struct emplace_lvalue_end_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.emplace_hint(x.end(), *it);
  }
} emplace_lvalue_end;

template <typename T> struct emplace_lvalue_pos_type_impl : inserter_base
{
  typename T::iterator pos;

  emplace_lvalue_pos_type_impl(T& x) : pos(x.begin()) {}

  template <typename Iterator> void operator()(T& x, Iterator it)
  {
    pos = get_iterator(x.emplace_hint(pos, *it));
  }
};

struct emplace_lvalue_pos_type
{
  template <typename T>
  friend emplace_lvalue_pos_type_impl<T> generate(emplace_lvalue_pos_type, T& x)
  {
    return emplace_lvalue_pos_type_impl<T>(x);
  }
} emplace_lvalue_pos;

// Run the exception tests in various combinations.

test_set* test_set_;
test_multiset* test_multiset_;
test_map* test_map_;
test_multimap* test_multimap_;

using test::default_generator;
using test::limited_range;
using test::generate_collisions;

// clang-format off
UNORDERED_TEST(insert_exception_test,
    ((test_set_)(test_multiset_)(test_map_)(test_multimap_))
    ((insert_lvalue)(insert_lvalue_begin)(insert_lvalue_end)
     (insert_lvalue_pos)(insert_single_item_range)
     (emplace_lvalue)(emplace_lvalue_begin)(emplace_lvalue_end)
     (emplace_lvalue_pos)
    )
    ((default_generator)(limited_range)(generate_collisions))
)

UNORDERED_TEST(insert_rehash_exception_test,
    ((test_set_)(test_multiset_)(test_map_)(test_multimap_))
    ((insert_lvalue)(insert_lvalue_begin)(insert_lvalue_end)
     (insert_lvalue_pos)(insert_single_item_range)
     (emplace_lvalue)(emplace_lvalue_begin)(emplace_lvalue_end)
     (emplace_lvalue_pos)
    )
    ((default_generator)(limited_range)(generate_collisions))
)
// clang-format on

// Repeat insert tests with pairs

struct pair_emplace_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.emplace(boost::unordered::piecewise_construct,
      boost::make_tuple(it->first), boost::make_tuple(it->second));
  }
} pair_emplace;

struct pair_emplace2_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.emplace_hint(x.begin(), boost::unordered::piecewise_construct,
      boost::make_tuple(it->first),
      boost::make_tuple(it->second.tag1_, it->second.tag2_));
  }
} pair_emplace2;

test_pair_set* test_pair_set_;
test_pair_multiset* test_pair_multiset_;

// clang-format off
UNORDERED_TEST(insert_exception_test,
    ((test_pair_set_)(test_pair_multiset_)(test_map_)(test_multimap_))
    ((pair_emplace)(pair_emplace2))
    ((default_generator)(limited_range)(generate_collisions))
)
UNORDERED_TEST(insert_rehash_exception_test,
    ((test_pair_set_)(test_pair_multiset_)(test_map_)(test_multimap_))
    ((pair_emplace)(pair_emplace2))
    ((default_generator)(limited_range)(generate_collisions))
)
// clang-format on

// Test inserting using operator[]

struct try_emplace_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.try_emplace(it->first, it->second);
  }
} try_emplace;

struct try_emplace2_type : inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.try_emplace(it->first, it->second.tag1_, it->second.tag2_);
  }
} try_emplace2;

struct map_inserter_base
{
  template <typename T> void exception_check(T& x, test::strong<T>& strong)
  {
    std::string scope(test::scope);

    if (scope.find("hash::operator()") == std::string::npos &&
        scope.find("::operator=") == std::string::npos)
      strong.test(x, test::detail::tracker.count_allocations);
  }

  template <typename T, typename Iterator>
  void track(T& tracker, Iterator begin, Iterator end)
  {
    for (; begin != end; ++begin) {
      tracker[begin->first] = begin->second;
    }
  }
};

struct map_insert_operator_type : map_inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x[it->first] = it->second;
  }
} map_insert_operator;

struct map_insert_or_assign_type : map_inserter_base
{
  template <typename T, typename Iterator> void operator()(T& x, Iterator it)
  {
    x.insert_or_assign(it->first, it->second);
  }
} map_insert_or_assign;

// clang-format off
UNORDERED_TEST(insert_exception_test,
    ((test_map_))
    ((try_emplace)(try_emplace2)(map_insert_operator)(map_insert_or_assign))
    ((default_generator)(limited_range)(generate_collisions))
)
UNORDERED_TEST(insert_rehash_exception_test,
    ((test_map_))
    ((try_emplace)(try_emplace2)(map_insert_operator)(map_insert_or_assign))
    ((default_generator)(limited_range)(generate_collisions))
)
// clang-format on

// Range insert tests

template <typename T, typename Values>
void insert_range_exception_test_impl(T x, Values const& v)
{
  test::ordered<T> tracker;
  tracker.insert(x.begin(), x.end());

  try {
    ENABLE_EXCEPTIONS;
    x.insert(v.begin(), v.end());
  } catch (...) {
    test::check_equivalent_keys(x);
    throw;
  }

  test::check_equivalent_keys(x);
  tracker.insert(v.begin(), v.end());
  tracker.compare(x);
}

template <typename T>
void insert_range_exception_test(T*, test::random_generator gen)
{
  for (int i = 0; i < 5; ++i) {
    test::random_values<T> v(10, gen);
    T x;

    EXCEPTION_LOOP(insert_range_exception_test_impl(x, v));
  }
}

template <typename T>
void insert_range_rehash_exception_test(T*, test::random_generator gen)
{
  for (int i = 0; i < 5; ++i) {
    T x;
    rehash_prep(x);

    test::random_values<T> v2(5, gen);
    EXCEPTION_LOOP(insert_range_exception_test_impl(x, v2));
  }
}

// clang-format off
UNORDERED_TEST(insert_range_exception_test,
    ((test_set_)(test_multiset_)(test_map_)(test_multimap_))
    ((default_generator)(limited_range)(generate_collisions))
)

UNORDERED_TEST(insert_range_rehash_exception_test,
    ((test_set_)(test_multiset_)(test_map_)(test_multimap_))
    ((default_generator)(limited_range)(generate_collisions))
)
// clang-format on

RUN_TESTS()
