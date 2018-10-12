//
// Copyright 2016 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include <boost/functional/hash/hash.hpp>
#include "../helpers/test.hpp"
#include "../helpers/count.hpp"
#include <string>

// Test that various emplace methods work with different numbers of
// arguments.

namespace emplace_tests {
  // Constructible with 2 to 10 arguments
  struct emplace_value : private test::counted_object
  {
    typedef int A0;
    typedef std::string A1;
    typedef char A2;
    typedef int A3;
    typedef int A4;
    typedef int A5;
    typedef int A6;
    typedef int A7;
    typedef int A8;
    typedef int A9;

    int arg_count;

    A0 a0;
    A1 a1;
    A2 a2;
    A3 a3;
    A4 a4;
    A5 a5;
    A6 a6;
    A7 a7;
    A8 a8;
    A9 a9;

    emplace_value(A0 const& b0, A1 const& b1) : arg_count(2), a0(b0), a1(b1) {}

    emplace_value(A0 const& b0, A1 const& b1, A2 const& b2)
        : arg_count(3), a0(b0), a1(b1), a2(b2)
    {
    }

    emplace_value(A0 const& b0, A1 const& b1, A2 const& b2, A3 const& b3)
        : arg_count(4), a0(b0), a1(b1), a2(b2), a3(b3)
    {
    }

    emplace_value(
      A0 const& b0, A1 const& b1, A2 const& b2, A3 const& b3, A4 const& b4)
        : arg_count(5), a0(b0), a1(b1), a2(b2), a3(b3), a4(b4)
    {
    }

    emplace_value(A0 const& b0, A1 const& b1, A2 const& b2, A3 const& b3,
      A4 const& b4, A5 const& b5)
        : arg_count(6), a0(b0), a1(b1), a2(b2), a3(b3), a4(b4), a5(b5)
    {
    }

    emplace_value(A0 const& b0, A1 const& b1, A2 const& b2, A3 const& b3,
      A4 const& b4, A5 const& b5, A6 const& b6)
        : arg_count(7), a0(b0), a1(b1), a2(b2), a3(b3), a4(b4), a5(b5), a6(b6)
    {
    }

    emplace_value(A0 const& b0, A1 const& b1, A2 const& b2, A3 const& b3,
      A4 const& b4, A5 const& b5, A6 const& b6, A7 const& b7)
        : arg_count(8), a0(b0), a1(b1), a2(b2), a3(b3), a4(b4), a5(b5), a6(b6),
          a7(b7)
    {
    }

    emplace_value(A0 const& b0, A1 const& b1, A2 const& b2, A3 const& b3,
      A4 const& b4, A5 const& b5, A6 const& b6, A7 const& b7, A8 const& b8)
        : arg_count(9), a0(b0), a1(b1), a2(b2), a3(b3), a4(b4), a5(b5), a6(b6),
          a7(b7), a8(b8)
    {
    }

    emplace_value(A0 const& b0, A1 const& b1, A2 const& b2, A3 const& b3,
      A4 const& b4, A5 const& b5, A6 const& b6, A7 const& b7, A8 const& b8,
      A9 const& b9)
        : arg_count(10), a0(b0), a1(b1), a2(b2), a3(b3), a4(b4), a5(b5), a6(b6),
          a7(b7), a8(b8), a9(b9)
    {
    }

    friend std::size_t hash_value(emplace_value const& x)
    {
      std::size_t r1 = 23894278u;
      if (x.arg_count >= 1)
        boost::hash_combine(r1, x.a0);
      if (x.arg_count >= 2)
        boost::hash_combine(r1, x.a1);
      if (x.arg_count >= 3)
        boost::hash_combine(r1, x.a2);
      if (x.arg_count >= 4)
        boost::hash_combine(r1, x.a3);
      if (x.arg_count >= 5)
        boost::hash_combine(r1, x.a4);
      if (x.arg_count >= 6)
        boost::hash_combine(r1, x.a5);
      if (x.arg_count >= 7)
        boost::hash_combine(r1, x.a6);
      if (x.arg_count >= 8)
        boost::hash_combine(r1, x.a7);
      if (x.arg_count >= 9)
        boost::hash_combine(r1, x.a8);
      if (x.arg_count >= 10)
        boost::hash_combine(r1, x.a9);
      return r1;
    }

    friend bool operator==(emplace_value const& x, emplace_value const& y)
    {
      if (x.arg_count != y.arg_count) {
        return false;
      }
      if (x.arg_count >= 1 && x.a0 != y.a0) {
        return false;
      }
      if (x.arg_count >= 2 && x.a1 != y.a1) {
        return false;
      }
      if (x.arg_count >= 3 && x.a2 != y.a2) {
        return false;
      }
      if (x.arg_count >= 4 && x.a3 != y.a3) {
        return false;
      }
      if (x.arg_count >= 5 && x.a4 != y.a4) {
        return false;
      }
      if (x.arg_count >= 6 && x.a5 != y.a5) {
        return false;
      }
      if (x.arg_count >= 7 && x.a6 != y.a6) {
        return false;
      }
      if (x.arg_count >= 8 && x.a7 != y.a7) {
        return false;
      }
      if (x.arg_count >= 9 && x.a8 != y.a8) {
        return false;
      }
      if (x.arg_count >= 10 && x.a9 != y.a9) {
        return false;
      }
      return true;
    }

  private:
    emplace_value();
    emplace_value(emplace_value const&);
  };

  UNORDERED_AUTO_TEST (emplace_set) {
    test::check_instances check_;

    typedef boost::unordered_set<emplace_value, boost::hash<emplace_value> >
      container;
    typedef container::iterator iterator;
    typedef std::pair<iterator, bool> return_type;
    container x(10);
    iterator i1;
    return_type r1, r2;

    // 2 args

    emplace_value v1(10, "x");
    r1 = x.emplace(10, std::string("x"));
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST(r1.second);
    BOOST_TEST(*r1.first == v1);
    BOOST_TEST(r1.first == x.find(v1));
    BOOST_TEST_EQ(check_.instances(), 2);
    BOOST_TEST_EQ(check_.constructions(), 2);

    // 3 args

    emplace_value v2(3, "foo", 'a');
    r1 = x.emplace(3, "foo", 'a');
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST(r1.second);
    BOOST_TEST(*r1.first == v2);
    BOOST_TEST(r1.first == x.find(v2));
    BOOST_TEST_EQ(check_.instances(), 4);
    BOOST_TEST_EQ(check_.constructions(), 4);

    // 7 args with hint + duplicate

    emplace_value v3(25, "something", 'z', 4, 5, 6, 7);
    i1 = x.emplace_hint(r1.first, 25, "something", 'z', 4, 5, 6, 7);
    BOOST_TEST_EQ(x.size(), 3u);
    BOOST_TEST(*i1 == v3);
    BOOST_TEST(i1 == x.find(v3));
    BOOST_TEST_EQ(check_.instances(), 6);
    BOOST_TEST_EQ(check_.constructions(), 6);

    r2 = x.emplace(25, "something", 'z', 4, 5, 6, 7);
    BOOST_TEST_EQ(x.size(), 3u);
    BOOST_TEST(!r2.second);
    BOOST_TEST(i1 == r2.first);
    // The container has to construct an object in order to check
    // whether it can emplace, so there's an extra cosntruction
    // here.
    BOOST_TEST_EQ(check_.instances(), 6);
    BOOST_TEST_EQ(check_.constructions(), 7);

    // 10 args + hint duplicate

    std::string s1;
    emplace_value v4(10, s1, 'a', 4, 5, 6, 7, 8, 9, 10);
    r1 = x.emplace(10, s1, 'a', 4, 5, 6, 7, 8, 9, 10);
    BOOST_TEST_EQ(x.size(), 4u);
    BOOST_TEST(r1.second);
    BOOST_TEST(*r1.first == v4);
    BOOST_TEST(r1.first == x.find(v4));
    BOOST_TEST_EQ(check_.instances(), 8);
    BOOST_TEST_EQ(check_.constructions(), 9);

    BOOST_TEST(
      r1.first == x.emplace_hint(r1.first, 10, "", 'a', 4, 5, 6, 7, 8, 9, 10));
    BOOST_TEST(
      r1.first == x.emplace_hint(r2.first, 10, "", 'a', 4, 5, 6, 7, 8, 9, 10));
    BOOST_TEST(
      r1.first == x.emplace_hint(x.end(), 10, "", 'a', 4, 5, 6, 7, 8, 9, 10));
    BOOST_TEST_EQ(check_.instances(), 8);
    BOOST_TEST_EQ(check_.constructions(), 12);

    BOOST_TEST_EQ(x.size(), 4u);
    BOOST_TEST(x.count(v1) == 1);
    BOOST_TEST(x.count(v2) == 1);
    BOOST_TEST(x.count(v3) == 1);
    BOOST_TEST(x.count(v4) == 1);
  }

  UNORDERED_AUTO_TEST (emplace_multiset) {
    test::check_instances check_;

    typedef boost::unordered_multiset<emplace_value,
      boost::hash<emplace_value> >
      container;
    typedef container::iterator iterator;
    container x(10);
    iterator i1, i2;

    // 2 args.

    emplace_value v1(10, "x");
    i1 = x.emplace(10, std::string("x"));
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST(i1 == x.find(v1));
    BOOST_TEST_EQ(check_.instances(), 2);
    BOOST_TEST_EQ(check_.constructions(), 2);

    // 4 args + duplicate

    emplace_value v2(4, "foo", 'a', 15);
    i1 = x.emplace(4, "foo", 'a', 15);
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST(i1 == x.find(v2));
    BOOST_TEST_EQ(check_.instances(), 4);
    BOOST_TEST_EQ(check_.constructions(), 4);

    i2 = x.emplace(4, "foo", 'a', 15);
    BOOST_TEST_EQ(x.size(), 3u);
    BOOST_TEST(i1 != i2);
    BOOST_TEST(*i1 == *i2);
    BOOST_TEST(x.count(*i1) == 2);
    BOOST_TEST_EQ(check_.instances(), 5);
    BOOST_TEST_EQ(check_.constructions(), 5);

    // 7 args + duplicate using hint.

    emplace_value v3(7, "", 'z', 4, 5, 6, 7);
    i1 = x.emplace(7, "", 'z', 4, 5, 6, 7);
    BOOST_TEST_EQ(x.size(), 4u);
    BOOST_TEST_EQ(i1->a2, 'z');
    BOOST_TEST(x.count(*i1) == 1);
    BOOST_TEST(i1 == x.find(v3));
    BOOST_TEST_EQ(check_.instances(), 7);
    BOOST_TEST_EQ(check_.constructions(), 7);

    i2 = x.emplace_hint(i1, 7, "", 'z', 4, 5, 6, 7);
    BOOST_TEST_EQ(x.size(), 5u);
    BOOST_TEST(*i1 == *i2);
    BOOST_TEST(i1 != i2);
    BOOST_TEST(x.count(*i1) == 2);
    BOOST_TEST_EQ(check_.instances(), 8);
    BOOST_TEST_EQ(check_.constructions(), 8);

    // 10 args with bad hint + duplicate

    emplace_value v4(10, "", 'a', 4, 5, 6, 7, 8, 9, 10);
    i1 = x.emplace_hint(i2, 10, "", 'a', 4, 5, 6, 7, 8, 9, 10);
    BOOST_TEST_EQ(x.size(), 6u);
    BOOST_TEST_EQ(i1->arg_count, 10);
    BOOST_TEST(i1 == x.find(v4));
    BOOST_TEST_EQ(check_.instances(), 10);
    BOOST_TEST_EQ(check_.constructions(), 10);

    i2 = x.emplace_hint(x.end(), 10, "", 'a', 4, 5, 6, 7, 8, 9, 10);
    BOOST_TEST_EQ(x.size(), 7u);
    BOOST_TEST(*i1 == *i2);
    BOOST_TEST(i1 != i2);
    BOOST_TEST(x.count(*i1) == 2);
    BOOST_TEST_EQ(check_.instances(), 11);
    BOOST_TEST_EQ(check_.constructions(), 11);

    BOOST_TEST_EQ(x.count(v1), 1u);
    BOOST_TEST_EQ(x.count(v2), 2u);
    BOOST_TEST_EQ(x.count(v3), 2u);
  }

  UNORDERED_AUTO_TEST (emplace_map) {
    test::check_instances check_;

    typedef boost::unordered_map<emplace_value, emplace_value,
      boost::hash<emplace_value> >
      container;
    typedef container::iterator iterator;
    typedef std::pair<iterator, bool> return_type;
    container x(10);
    return_type r1, r2;

    // 5/8 args + duplicate

    emplace_value k1(5, "", 'b', 4, 5);
    emplace_value m1(8, "xxx", 'z', 4, 5, 6, 7, 8);
    r1 = x.emplace(boost::unordered::piecewise_construct,
      boost::make_tuple(5, "", 'b', 4, 5),
      boost::make_tuple(8, "xxx", 'z', 4, 5, 6, 7, 8));
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST(r1.second);
    BOOST_TEST(x.find(k1) == r1.first);
    BOOST_TEST(x.find(k1)->second == m1);
    BOOST_TEST_EQ(check_.instances(), 4);
    BOOST_TEST_EQ(check_.constructions(), 4);

    r2 = x.emplace(boost::unordered::piecewise_construct,
      boost::make_tuple(5, "", 'b', 4, 5),
      boost::make_tuple(8, "xxx", 'z', 4, 5, 6, 7, 8));
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST(!r2.second);
    BOOST_TEST(r1.first == r2.first);
    BOOST_TEST(x.find(k1)->second == m1);
    BOOST_TEST_EQ(check_.instances(), 4);
    // constructions could possibly be 5 if the implementation only
    // constructed the key.
    BOOST_TEST_EQ(check_.constructions(), 6);

    // 9/3 args + duplicates with hints, different mapped value.

    emplace_value k2(9, "", 'b', 4, 5, 6, 7, 8, 9);
    emplace_value m2(3, "aaa", 'm');
    r1 = x.emplace(boost::unordered::piecewise_construct,
      boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
      boost::make_tuple(3, "aaa", 'm'));
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST(r1.second);
    BOOST_TEST(r1.first->first.arg_count == 9);
    BOOST_TEST(r1.first->second.arg_count == 3);
    BOOST_TEST(x.find(k2) == r1.first);
    BOOST_TEST(x.find(k2)->second == m2);
    BOOST_TEST_EQ(check_.instances(), 8);
    BOOST_TEST_EQ(check_.constructions(), 10);

    BOOST_TEST(r1.first ==
               x.emplace_hint(r1.first, boost::unordered::piecewise_construct,
                 boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
                 boost::make_tuple(15, "jkjk")));
    BOOST_TEST(r1.first ==
               x.emplace_hint(r2.first, boost::unordered::piecewise_construct,
                 boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
                 boost::make_tuple(275, "xxx", 'm', 6)));
    BOOST_TEST(r1.first ==
               x.emplace_hint(x.end(), boost::unordered::piecewise_construct,
                 boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
                 boost::make_tuple(-10, "blah blah", '\0')));
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST(x.find(k2)->second == m2);
    BOOST_TEST_EQ(check_.instances(), 8);
    BOOST_TEST_EQ(check_.constructions(), 16);
  }

  UNORDERED_AUTO_TEST (emplace_multimap) {
    test::check_instances check_;

    typedef boost::unordered_multimap<emplace_value, emplace_value,
      boost::hash<emplace_value> >
      container;
    typedef container::iterator iterator;
    container x(10);
    iterator i1, i2, i3, i4;

    // 5/8 args + duplicate

    emplace_value k1(5, "", 'b', 4, 5);
    emplace_value m1(8, "xxx", 'z', 4, 5, 6, 7, 8);
    i1 = x.emplace(boost::unordered::piecewise_construct,
      boost::make_tuple(5, "", 'b', 4, 5),
      boost::make_tuple(8, "xxx", 'z', 4, 5, 6, 7, 8));
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST(x.find(k1) == i1);
    BOOST_TEST(x.find(k1)->second == m1);
    BOOST_TEST_EQ(check_.instances(), 4);
    BOOST_TEST_EQ(check_.constructions(), 4);

    emplace_value m1a(8, "xxx", 'z', 4, 5, 6, 7, 8);
    i2 = x.emplace(boost::unordered::piecewise_construct,
      boost::make_tuple(5, "", 'b', 4, 5),
      boost::make_tuple(8, "xxx", 'z', 4, 5, 6, 7, 8));
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST(i1 != i2);
    BOOST_TEST(i1->second == m1);
    BOOST_TEST(i2->second == m1a);
    BOOST_TEST_EQ(check_.instances(), 7);
    BOOST_TEST_EQ(check_.constructions(), 7);

    // 9/3 args + duplicates with hints, different mapped value.

    emplace_value k2(9, "", 'b', 4, 5, 6, 7, 8, 9);
    emplace_value m2(3, "aaa", 'm');
    i1 = x.emplace(boost::unordered::piecewise_construct,
      boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
      boost::make_tuple(3, "aaa", 'm'));
    BOOST_TEST_EQ(x.size(), 3u);
    BOOST_TEST(i1->first.arg_count == 9);
    BOOST_TEST(i1->second.arg_count == 3);
    BOOST_TEST_EQ(check_.instances(), 11);
    BOOST_TEST_EQ(check_.constructions(), 11);

    emplace_value m2a(15, "jkjk");
    i2 = x.emplace_hint(i2, boost::unordered::piecewise_construct,
      boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
      boost::make_tuple(15, "jkjk"));
    emplace_value m2b(275, "xxx", 'm', 6);
    i3 = x.emplace_hint(i1, boost::unordered::piecewise_construct,
      boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
      boost::make_tuple(275, "xxx", 'm', 6));
    emplace_value m2c(-10, "blah blah", '\0');
    i4 = x.emplace_hint(x.end(), boost::unordered::piecewise_construct,
      boost::make_tuple(9, "", 'b', 4, 5, 6, 7, 8, 9),
      boost::make_tuple(-10, "blah blah", '\0'));
    BOOST_TEST_EQ(x.size(), 6u);
    BOOST_TEST(x.find(k2)->second == m2);
    BOOST_TEST_EQ(check_.instances(), 20);
    BOOST_TEST_EQ(check_.constructions(), 20);
  }

  UNORDERED_AUTO_TEST (try_emplace) {
    test::check_instances check_;

    typedef boost::unordered_map<int, emplace_value> container;
    typedef container::iterator iterator;
    typedef std::pair<iterator, bool> return_type;
    container x(10);
    return_type r1, r2, r3;

    int k1 = 3;
    emplace_value m1(414, "grr");
    r1 = x.try_emplace(3, 414, "grr");
    BOOST_TEST(r1.second);
    BOOST_TEST(r1.first->first == k1);
    BOOST_TEST(r1.first->second == m1);
    BOOST_TEST_EQ(x.size(), 1u);
    BOOST_TEST_EQ(check_.instances(), 2);
    BOOST_TEST_EQ(check_.constructions(), 2);

    int k2 = 10;
    emplace_value m2(25, "", 'z');
    r2 = x.try_emplace(10, 25, std::string(""), 'z');
    BOOST_TEST(r2.second);
    BOOST_TEST(r2.first->first == k2);
    BOOST_TEST(r2.first->second == m2);
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST_EQ(check_.instances(), 4);
    BOOST_TEST_EQ(check_.constructions(), 4);

    BOOST_TEST(x.find(k1)->second == m1);
    BOOST_TEST(x.find(k2)->second == m2);

    r3 = x.try_emplace(k2, 68, "jfeoj", 'p', 49309, 2323);
    BOOST_TEST(!r3.second);
    BOOST_TEST(r3.first == r2.first);
    BOOST_TEST(r3.first->second == m2);
    BOOST_TEST_EQ(x.size(), 2u);
    BOOST_TEST_EQ(check_.instances(), 4);
    BOOST_TEST_EQ(check_.constructions(), 4);

    BOOST_TEST(r2.first == x.try_emplace(r2.first, k2, 808709, "what"));
    BOOST_TEST(
      r2.first ==
      x.try_emplace(r2.first, k2, 10, "xxx", 'a', 4, 5, 6, 7, 8, 9, 10));
    BOOST_TEST(r2.first->second == m2);
    BOOST_TEST_EQ(x.size(), 2u);
  }
}

RUN_TESTS()
