
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "./containers.hpp"

#include "../helpers/invariants.hpp"
#include "../helpers/random_values.hpp"
#include "../helpers/tracker.hpp"

#if defined(BOOST_MSVC)
#pragma warning(                                                               \
  disable : 4512) // move_assignment operator could not be generated
#endif

test::seed_t initialize_seed(12847);

template <class T> struct move_assign_base : public test::exception_base
{
  test::random_values<T> x_values, y_values;
  T x, y;

  typedef typename T::hasher hasher;
  typedef typename T::key_equal key_equal;
  typedef typename T::allocator_type allocator_type;

  move_assign_base(int tag1, int tag2, float mlf1 = 1.0, float mlf2 = 1.0)
      : x_values(), y_values(),
        x(0, hasher(tag1), key_equal(tag1), allocator_type(tag1)),
        y(0, hasher(tag2), key_equal(tag2), allocator_type(tag2))
  {
    x.max_load_factor(mlf1);
    y.max_load_factor(mlf2);
  }

  typedef T data_type;
  T init() const { return T(x); }
  void run(T& x1) const
  {
    test::exceptions_enable disable_exceptions(false);
    T y1 = y;
    disable_exceptions.release();
    x1 = boost::move(y1);

    DISABLE_EXCEPTIONS;
    test::check_container(x1, y_values);
    test::check_equivalent_keys(x1);
  }

  void check BOOST_PREVENT_MACRO_SUBSTITUTION(T const& x1) const
  {
    test::check_equivalent_keys(x1);

    // If the container is empty at the point of the exception, the
    // internal structure is hidden, this exposes it, at the cost of
    // messing up the data.
    if (x_values.size()) {
      T& x2 = const_cast<T&>(x1);
      x2.emplace(*x_values.begin());
      test::check_equivalent_keys(x2);
    }
  }
};

template <class T> struct move_assign_values : move_assign_base<T>
{
  move_assign_values(unsigned int count1, unsigned int count2, int tag1,
    int tag2, float mlf1 = 1.0, float mlf2 = 1.0)
      : move_assign_base<T>(tag1, tag2, mlf1, mlf2)
  {
    this->x_values.fill(count1, test::limited_range);
    this->y_values.fill(count2, test::limited_range);
    this->x.insert(this->x_values.begin(), this->x_values.end());
    this->y.insert(this->y_values.begin(), this->y_values.end());
  }
};

template <class T> struct move_assign_test1 : move_assign_values<T>
{
  move_assign_test1() : move_assign_values<T>(0, 0, 0, 0) {}
};

template <class T> struct move_assign_test2 : move_assign_values<T>
{
  move_assign_test2() : move_assign_values<T>(60, 0, 0, 0) {}
};

template <class T> struct move_assign_test3 : move_assign_values<T>
{
  move_assign_test3() : move_assign_values<T>(0, 60, 0, 0) {}
};

template <class T> struct move_assign_test4 : move_assign_values<T>
{
  move_assign_test4() : move_assign_values<T>(10, 10, 1, 2) {}
};

template <class T> struct move_assign_test4a : move_assign_values<T>
{
  move_assign_test4a() : move_assign_values<T>(10, 100, 1, 2) {}
};

template <class T> struct move_assign_test5 : move_assign_values<T>
{
  move_assign_test5() : move_assign_values<T>(5, 60, 0, 0, 1.0f, 0.1f) {}
};

template <class T> struct equivalent_test1 : move_assign_base<T>
{
  equivalent_test1() : move_assign_base<T>(0, 0)
  {
    test::random_values<T> x_values2(10, test::limited_range);
    this->x_values.insert(x_values2.begin(), x_values2.end());
    this->x_values.insert(x_values2.begin(), x_values2.end());
    test::random_values<T> y_values2(10, test::limited_range);
    this->y_values.insert(y_values2.begin(), y_values2.end());
    this->y_values.insert(y_values2.begin(), y_values2.end());
    this->x.insert(this->x_values.begin(), this->x_values.end());
    this->y.insert(this->y_values.begin(), this->y_values.end());
  }
};

// clang-format off
EXCEPTION_TESTS(
  (move_assign_test1)(move_assign_test2)(move_assign_test3)
  (move_assign_test4)(move_assign_test4a)(move_assign_test5)
  (equivalent_test1),
  CONTAINER_SEQ)
// clang-format on

RUN_TESTS()
