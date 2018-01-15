
// Copyright 2005-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(BOOST_UNORDERED_TEST_HELPERS_METAFUNCTIONS_HEADER)
#define BOOST_UNORDERED_TEST_HELPERS_METAFUNCTIONS_HEADER

#include <boost/config.hpp>
#include <boost/type_traits/is_same.hpp>

namespace test {
  template <class Container>
  struct is_set
    : public boost::is_same<BOOST_DEDUCED_TYPENAME Container::key_type,
        BOOST_DEDUCED_TYPENAME Container::value_type>
  {
  };

  template <class Container> struct has_unique_keys
  {
    static char flip(BOOST_DEDUCED_TYPENAME Container::iterator const&);
    static long flip(
      std::pair<BOOST_DEDUCED_TYPENAME Container::iterator, bool> const&);
    BOOST_STATIC_CONSTANT(bool,
      value = sizeof(long) ==
              sizeof(
                flip(((Container*)0)
                       ->insert(
                         *(BOOST_DEDUCED_TYPENAME Container::value_type*)0))));
  };
}

#endif
