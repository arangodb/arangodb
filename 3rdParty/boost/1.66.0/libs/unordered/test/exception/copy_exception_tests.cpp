
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "./containers.hpp"

#include "../helpers/invariants.hpp"
#include "../helpers/random_values.hpp"
#include "../helpers/tracker.hpp"

template <typename T> inline void avoid_unused_warning(T const&) {}

test::seed_t initialize_seed(73041);

template <class T> struct copy_test1 : public test::exception_base
{
  T x;

  void run() const
  {
    T y(x);

    DISABLE_EXCEPTIONS;
    BOOST_TEST(y.empty());
    test::check_equivalent_keys(y);
  }
};

template <class T> struct copy_test2 : public test::exception_base
{
  test::random_values<T> values;
  T x;

  copy_test2() : values(5, test::limited_range), x(values.begin(), values.end())
  {
  }

  void run() const
  {
    T y(x);

    DISABLE_EXCEPTIONS;
    test::check_container(y, this->values);
    test::check_equivalent_keys(y);
  }
};

template <class T> struct copy_test3 : public test::exception_base
{
  test::random_values<T> values;
  T x;

  copy_test3() : values(100), x(values.begin(), values.end()) {}

  void run() const
  {
    T y(x);

    DISABLE_EXCEPTIONS;
    test::check_container(y, this->values);
    test::check_equivalent_keys(y);
  }
};

template <class T> struct copy_test3a : public test::exception_base
{
  test::random_values<T> values;
  T x;

  copy_test3a()
      : values(100, test::limited_range), x(values.begin(), values.end())
  {
  }

  void run() const
  {
    T y(x);

    DISABLE_EXCEPTIONS;
    test::check_container(y, this->values);
    test::check_equivalent_keys(y);
  }
};

template <class T> struct copy_with_allocator_test : public test::exception_base
{
  test::random_values<T> values;
  T x;
  test::exception::allocator<test::exception::object> allocator;

  copy_with_allocator_test() : values(100), x(values.begin(), values.end()) {}

  void run() const
  {
    T y(x, allocator);

    DISABLE_EXCEPTIONS;
    test::check_container(y, this->values);
    test::check_equivalent_keys(y);
  }
};

// clang-format off
EXCEPTION_TESTS(
    (copy_test1)(copy_test2)(copy_test3)(copy_test3a)(copy_with_allocator_test),
    CONTAINER_SEQ)
// clang-format on

RUN_TESTS()
