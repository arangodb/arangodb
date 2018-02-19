
// Copyright 2013 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/test.hpp"

namespace noexcept_tests {
  // Test the noexcept is set correctly for the move constructor.

  struct hash_possible_exception : boost::hash<int>
  {
    hash_possible_exception(hash_possible_exception const&) {}
  };

  struct equal_to_possible_exception : std::equal_to<int>
  {
    equal_to_possible_exception(equal_to_possible_exception const&) {}
  };

  // Test that the move constructor does actually move without throwing
  // an exception when it claims to.

  struct test_exception
  {
  };

  bool throwing_test_exception = false;
  void test_throw(char const* name)
  {
    if (throwing_test_exception) {
      BOOST_LIGHTWEIGHT_TEST_OSTREAM << "Throw exception in: " << name
                                     << std::endl;
      throw test_exception();
    }
  }

  class hash_nothrow_move : boost::hash<int>
  {
    BOOST_COPYABLE_AND_MOVABLE(hash_nothrow_move)

    typedef boost::hash<int> base;

  public:
    hash_nothrow_move(BOOST_RV_REF(hash_nothrow_move)) BOOST_NOEXCEPT {}

    hash_nothrow_move() { test_throw("Constructor"); }
    hash_nothrow_move(hash_nothrow_move const&) { test_throw("Copy"); }
    hash_nothrow_move& operator=(BOOST_COPY_ASSIGN_REF(hash_nothrow_move))
    {
      test_throw("Assign");
      return *this;
    }
    hash_nothrow_move& operator=(BOOST_RV_REF(hash_nothrow_move))
    {
      test_throw("Move Assign");
      return *this;
    }
    std::size_t operator()(int x) const
    {
      test_throw("Operator");
      return static_cast<base const&>(*this)(x);
    }
  };

  class equal_to_nothrow_move : std::equal_to<int>
  {
    BOOST_COPYABLE_AND_MOVABLE(equal_to_nothrow_move)

    typedef std::equal_to<int> base;

  public:
    equal_to_nothrow_move(BOOST_RV_REF(equal_to_nothrow_move)) BOOST_NOEXCEPT {}
    equal_to_nothrow_move() { test_throw("Constructor"); }
    equal_to_nothrow_move(equal_to_nothrow_move const&) { test_throw("Copy"); }
    equal_to_nothrow_move& operator=(
      BOOST_COPY_ASSIGN_REF(equal_to_nothrow_move))
    {
      test_throw("Assign");
      return *this;
    }
    equal_to_nothrow_move& operator=(BOOST_RV_REF(equal_to_nothrow_move))
    {
      test_throw("Move Assign");
      return *this;
    }
    std::size_t operator()(int x, int y) const
    {
      test_throw("Operator");
      return static_cast<base const&>(*this)(x, y);
    }
  };

  bool have_is_nothrow_move = false;

  UNORDERED_AUTO_TEST (check_is_nothrow_move) {
    BOOST_TEST(
      !boost::is_nothrow_move_constructible<hash_possible_exception>::value);
    have_is_nothrow_move =
      boost::is_nothrow_move_constructible<hash_nothrow_move>::value;

// Copied from boost::is_nothrow_move_constructible implementation
// to make sure this does actually detect it when expected.
//
// The type trait is also available when BOOST_IS_NOTHROW_MOVE_CONSTRUCT
// is defined (for some versions of Visual C++?) but detects 'throw()',
// not noexcept.
#if !defined(BOOST_NO_CXX11_NOEXCEPT) && !defined(BOOST_NO_SFINAE_EXPR) &&     \
  !BOOST_WORKAROUND(BOOST_GCC_VERSION, < 40800)
    BOOST_TEST(have_is_nothrow_move);
#endif
  }

  UNORDERED_AUTO_TEST (test_noexcept) {
    if (have_is_nothrow_move) {
      BOOST_TEST((boost::is_nothrow_move_constructible<
        boost::unordered_set<int> >::value));
      BOOST_TEST((boost::is_nothrow_move_constructible<
        boost::unordered_multiset<int> >::value));
      BOOST_TEST((boost::is_nothrow_move_constructible<
        boost::unordered_map<int, int> >::value));
      BOOST_TEST((boost::is_nothrow_move_constructible<
        boost::unordered_multimap<int, int> >::value));
    }

    BOOST_TEST((!boost::is_nothrow_move_constructible<
                boost::unordered_set<int, hash_possible_exception> >::value));
    BOOST_TEST(
      (!boost::is_nothrow_move_constructible<boost::unordered_multiset<int,
          boost::hash<int>, equal_to_possible_exception> >::value));
  }

  UNORDERED_AUTO_TEST (test_no_throw_when_noexcept) {
    typedef boost::unordered_set<int, hash_nothrow_move, equal_to_nothrow_move>
      throwing_set;

    if (have_is_nothrow_move) {
      BOOST_TEST(boost::is_nothrow_move_constructible<throwing_set>::value);

      throwing_test_exception = false;

      throwing_set x1;
      x1.insert(10);
      x1.insert(50);

      try {
        throwing_test_exception = true;

        throwing_set x2 = boost::move(x1);
        BOOST_TEST(x2.size() == 2);
        BOOST_TEST(*x2.begin() == 10 || *x2.begin() == 50);
      } catch (test_exception) {
        BOOST_TEST(false);
      }

      throwing_test_exception = false;
    }
  }
}

RUN_TESTS()
