
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(BOOST_UNORDERED_TEST_HELPERS_HEADER)
#define BOOST_UNORDERED_TEST_HELPERS_HEADER

#include <iterator>

namespace test {
  template <class Container> struct get_key_impl
  {
    typedef typename Container::key_type key_type;

    static key_type const& get_key(key_type const& x) { return x; }

    template <class T>
    static key_type const& get_key(std::pair<key_type, T> const& x, char = 0)
    {
      return x.first;
    }

    template <class T>
    static key_type const& get_key(
      std::pair<key_type const, T> const& x, unsigned char = 0)
    {
      return x.first;
    }
  };

  template <class Container, class T>
  inline typename Container::key_type const& get_key(T const& x)
  {
    return get_key_impl<Container>::get_key(x);
  }

  // test::next
  //
  // Increments an iterator by 1 or a given value.
  // Like boost::next, but simpler.
  // Mainly because boost::next uses an MPL file
  // which causes warnings.

  template <typename Iterator> Iterator next(Iterator it) { return ++it; }

  template <typename Iterator, typename IntType>
  Iterator next(Iterator it, IntType x)
  {
    std::advance(it,
      static_cast<typename std::iterator_traits<Iterator>::difference_type>(x));
    return it;
  }
}

#endif
