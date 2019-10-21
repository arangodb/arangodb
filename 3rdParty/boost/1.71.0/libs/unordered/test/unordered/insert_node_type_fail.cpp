
// Copyright 2017 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

int main()
{
#if defined(UNORDERED_TEST_MAP)
  typedef boost::unordered_map<int, int> container;
  container x;
  x.emplace(1, 1);
#elif defined(UNORDERED_TEST_MULTIMAP)
  typedef boost::unordered_multimap<int, int> container;
  container x;
#elif defined(UNORDERED_TEST_SET)
  typedef boost::unordered_set<int> container;
  container x;
  x.emplace(1);
#elif defined(UNORDERED_TEST_MULTISET)
  typedef boost::unordered_multiset<int> container;
  container x;
  x.emplace(1);
#else
#define UNORDERED_ERROR
#endif

#if !defined(UNORDERED_ERROR)
  container::node_type n = x.extract(x.begin());
  x.insert(n);
#endif
}
