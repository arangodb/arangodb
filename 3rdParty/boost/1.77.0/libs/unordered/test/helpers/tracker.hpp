
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This header contains metafunctions/functions to get the equivalent
// associative container for an unordered container, and compare the contents.

#if !defined(BOOST_UNORDERED_TEST_HELPERS_TRACKER_HEADER)
#define BOOST_UNORDERED_TEST_HELPERS_TRACKER_HEADER

#include "../objects/fwd.hpp"
#include "./equivalent.hpp"
#include "./helpers.hpp"
#include "./list.hpp"
#include "./metafunctions.hpp"
#include <algorithm>
#include <iterator>
#include <map>
#include <set>

namespace test {
  template <typename X> struct equals_to_compare
  {
    typedef std::less<typename X::first_argument_type> type;
  };

  template <> struct equals_to_compare<test::equal_to>
  {
    typedef test::less type;
  };

  template <class X1, class X2> void compare_range(X1 const& x1, X2 const& x2)
  {
    typedef test::list<typename X1::value_type> value_list;
    value_list values1(x1.begin(), x1.end());
    value_list values2(x2.begin(), x2.end());
    values1.sort();
    values2.sort();
    BOOST_TEST(values1.size() == values2.size() &&
               test::equal(values1.begin(), values1.end(), values2.begin(),
                 test::equivalent));
  }

  template <class X1, class X2, class T>
  void compare_pairs(X1 const& x1, X2 const& x2, T*)
  {
    test::list<T> values1(x1.first, x1.second);
    test::list<T> values2(x2.first, x2.second);
    values1.sort();
    values2.sort();
    BOOST_TEST(values1.size() == values2.size() &&
               test::equal(values1.begin(), values1.end(), values2.begin(),
                 test::equivalent));
  }

  template <typename X, bool is_set = test::is_set<X>::value,
    bool has_unique_keys = test::has_unique_keys<X>::value>
  struct ordered_base;

  template <typename X> struct ordered_base<X, true, true>
  {
    typedef std::set<typename X::value_type,
      typename equals_to_compare<typename X::key_equal>::type>
      type;
  };

  template <typename X> struct ordered_base<X, true, false>
  {
    typedef std::multiset<typename X::value_type,
      typename equals_to_compare<typename X::key_equal>::type>
      type;
  };

  template <typename X> struct ordered_base<X, false, true>
  {
    typedef std::map<typename X::key_type, typename X::mapped_type,
      typename equals_to_compare<typename X::key_equal>::type>
      type;
  };

  template <typename X> struct ordered_base<X, false, false>
  {
    typedef std::multimap<typename X::key_type, typename X::mapped_type,
      typename equals_to_compare<typename X::key_equal>::type>
      type;
  };

  template <class X> class ordered : public ordered_base<X>::type
  {
    typedef typename ordered_base<X>::type base;

  public:
    typedef typename base::key_compare key_compare;

    ordered() : base() {}

    explicit ordered(key_compare const& kc) : base(kc) {}

    void compare(X const& x) { compare_range(x, *this); }

    void compare_key(X const& x, typename X::value_type const& val)
    {
      compare_pairs(x.equal_range(get_key<X>(val)),
        this->equal_range(get_key<X>(val)), (typename X::value_type*)0);
    }

    template <class It> void insert_range(It b, It e)
    {
      while (b != e) {
        this->insert(*b);
        ++b;
      }
    }
  };

  template <class Equals>
  typename equals_to_compare<Equals>::type create_compare(Equals const&)
  {
    typename equals_to_compare<Equals>::type x;
    return x;
  }

  template <class X> ordered<X> create_ordered(X const& container)
  {
    return ordered<X>(create_compare(container.key_eq()));
  }

  template <class X1, class X2>
  void check_container(X1 const& container, X2 const& values)
  {
    ordered<X1> tracker = create_ordered(container);
    tracker.insert_range(values.begin(), values.end());
    tracker.compare(container);
  }
}

#endif
