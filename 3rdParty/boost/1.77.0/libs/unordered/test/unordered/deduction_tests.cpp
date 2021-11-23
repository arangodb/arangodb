
// Copyright 2017-2018 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/unordered_map.hpp>
#include <iostream>
#include <vector>

#if BOOST_UNORDERED_TEMPLATE_DEDUCTION_GUIDES
struct hash_equals
{
  template <typename T> bool operator()(T const& x) const
  {
    boost::hash<T> hf;
    return hf(x);
  }

  template <typename T> bool operator()(T const& x, T const& y) const
  {
    std::equal_to<T> eq;
    return eq(x, y);
  }
};

template <typename T> struct test_allocator
{
  typedef T value_type;
  test_allocator() = default;
  template <typename T2> test_allocator(test_allocator<T2> const&) {}
  T* allocate(std::size_t n) const { return (T*)malloc(sizeof(T) * n); }
  void deallocate(T* ptr, std::size_t) const { free(ptr); }
  bool operator==(test_allocator const&) { return true; }
  bool operator!=(test_allocator const&) { return false; }
};
#endif

int main()
{
  std::cout << "BOOST_UNORDERED_TEMPLATE_DEDUCTION_GUIDES: "
            << BOOST_UNORDERED_TEMPLATE_DEDUCTION_GUIDES << std::endl;

#if BOOST_UNORDERED_TEMPLATE_DEDUCTION_GUIDES
  std::vector<std::pair<int, int> > x;
  x.push_back(std::make_pair(1, 3));
  x.push_back(std::make_pair(5, 10));
  test_allocator<std::pair<const int, int> > pair_allocator;
  hash_equals f;

  // unordered_map

  /*
   template<class InputIterator,
           class Hash = hash<iter_key_t<InputIterator>>,
           class Pred = equal_to<iter_key_t<InputIterator>>,
           class Allocator = allocator<iter_to_alloc_t<InputIterator>>>
    unordered_map(InputIterator, InputIterator, typename see below::size_type =
   see below,
                  Hash = Hash(), Pred = Pred(), Allocator = Allocator())
      -> unordered_map<iter_key_t<InputIterator>, iter_val_t<InputIterator>,
   Hash, Pred,
                       Allocator>;
  */

  {
    boost::unordered_map m(x.begin(), x.end());
    static_assert(
      std::is_same<decltype(m), boost::unordered_map<int, int> >::value);
  }

  /* Ambiguous:
  {
    boost::unordered_map m(x.begin(), x.end(), 0, std::hash<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_map<int, int,
  std::hash<int>>>::value);
  }

  {
    boost::unordered_map m(x.begin(), x.end(), 0, std::hash<int>(),
  std::equal_to<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_map<int, int,
  std::hash<int>, std::equal_to<int>>>::value);
  }
  */

  {
    boost::unordered_map m(x.begin(), x.end(), 0, std::hash<int>(),
      std::equal_to<int>(), pair_allocator);
    static_assert(std::is_same<decltype(m),
      boost::unordered_map<int, int, std::hash<int>, std::equal_to<int>,
        test_allocator<std::pair<const int, int> > > >::value);
  }

  /*
  template<class Key, class T, class Hash = hash<Key>,
           class Pred = equal_to<Key>, class Allocator = allocator<pair<const
  Key, T>>>
    unordered_map(initializer_list<pair<const Key, T>>,
                  typename see below::size_type = see below, Hash = Hash(),
                  Pred = Pred(), Allocator = Allocator())
      -> unordered_map<Key, T, Hash, Pred, Allocator>;
  */

  {
    boost::unordered_map m({std::pair<int const, int>(1, 2)});
    static_assert(
      std::is_same<decltype(m), boost::unordered_map<int, int> >::value);
  }

  /* Ambiguous
  {
    boost::unordered_map m({std::pair<int const, int>(1,2)}, 0,
  std::hash<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_map<int, int,
  std::hash<int>>>::value);
  }

  {
    boost::unordered_map m({std::pair<int const, int>(1,2)}, 0,
  std::hash<int>(), std::equal_to<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_map<int, int,
  std::hash<int>, std::equal_to<int>>>::value);
  }
  */

  {
    boost::unordered_map m(
      {std::pair<int const, int>(1, 2)}, 0, f, f, pair_allocator);
    static_assert(std::is_same<decltype(m),
      boost::unordered_map<int, int, hash_equals, hash_equals,
        test_allocator<std::pair<const int, int> > > >::value);
  }

  /*
  template<class InputIterator, class Allocator>
    unordered_map(InputIterator, InputIterator, typename see below::size_type,
  Allocator)
      -> unordered_map<iter_key_t<InputIterator>, iter_val_t<InputIterator>,
                       hash<iter_key_t<InputIterator>>,
  equal_to<iter_key_t<InputIterator>>,
                       Allocator>;
  */

  /* Ambiguous
  {
    boost::unordered_map m(x.begin(), x.end(), 0u, pair_allocator);
    static_assert(std::is_same<decltype(m), boost::unordered_map<int, int,
  boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  /*
    template<class InputIterator, class Allocator>
    unordered_map(InputIterator, InputIterator, Allocator)
      -> unordered_map<iter_key_t<InputIterator>, iter_val_t<InputIterator>,
                       hash<iter_key_t<InputIterator>>,
    equal_to<iter_key_t<InputIterator>>,
                       Allocator>;
  */

  /* No constructor:
  {
    boost::unordered_map m(x.begin(), x.end(), pair_allocator);
    static_assert(std::is_same<decltype(m), boost::unordered_map<int, int,
  boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  /*
  template<class InputIterator, class Hash, class Allocator>
    unordered_map(InputIterator, InputIterator, typename see below::size_type,
  Hash, Allocator)
      -> unordered_map<iter_key_t<InputIterator>, iter_val_t<InputIterator>,
  Hash,
                       equal_to<iter_key_t<InputIterator>>, Allocator>;
  */

  /* Ambiguous
  {
    boost::unordered_map m(x.begin(), x.end(), 0u, f, pair_allocator);
    static_assert(std::is_same<decltype(m), boost::unordered_map<int, int,
  hash_equals, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  /*
    template<class Key, class T, typename Allocator>
    unordered_map(initializer_list<pair<const Key, T>>, typename see
    below::size_type,
                  Allocator)
      -> unordered_map<Key, T, hash<Key>, equal_to<Key>, Allocator>;
  */

  /* Ambiguous
  {
    boost::unordered_map m({std::pair<int const, int>(1,2)}, 0, pair_allocator);
    static_assert(std::is_same<decltype(m),boost::unordered_map<int, int,
  boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  /*
  template<class Key, class T, typename Allocator>
    unordered_map(initializer_list<pair<const Key, T>>, Allocator)
      -> unordered_map<Key, T, hash<Key>, equal_to<Key>, Allocator>;
  */

  {
    boost::unordered_map m({std::pair<int const, int>(1, 2)}, pair_allocator);
    static_assert(std::is_same<decltype(m),
      boost::unordered_map<int, int, boost::hash<int>, std::equal_to<int>,
        test_allocator<std::pair<const int, int> > > >::value);
  }

  /*
  template<class Key, class T, class Hash, class Allocator>
    unordered_map(initializer_list<pair<const Key, T>>, typename see
  below::size_type, Hash,
                  Allocator)
      -> unordered_map<Key, T, Hash, equal_to<Key>, Allocator>;
  */

  /* Ambiguous
  {
    boost::unordered_map m({std::pair<int const, int>(1,2)}, 0, f,
  pair_allocator);
    static_assert(std::is_same<decltype(m),boost::unordered_map<int, int,
  boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  // unordered_multimap

  {
    boost::unordered_multimap m(x.begin(), x.end());
    static_assert(
      std::is_same<decltype(m), boost::unordered_multimap<int, int> >::value);
  }

  /* Ambiguous:
  {
    boost::unordered_multimap m(x.begin(), x.end(), 0, std::hash<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_multimap<int, int,
  std::hash<int>>>::value);
  }

  {
    boost::unordered_multimap m(x.begin(), x.end(), 0, std::hash<int>(),
  std::equal_to<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_multimap<int, int,
  std::hash<int>, std::equal_to<int>>>::value);
  }
  */

  {
    boost::unordered_multimap m(x.begin(), x.end(), 0, std::hash<int>(),
      std::equal_to<int>(), pair_allocator);
    static_assert(std::is_same<decltype(m),
      boost::unordered_multimap<int, int, std::hash<int>, std::equal_to<int>,
        test_allocator<std::pair<const int, int> > > >::value);
  }

  {
    boost::unordered_multimap m({std::pair<int const, int>(1, 2)});
    static_assert(
      std::is_same<decltype(m), boost::unordered_multimap<int, int> >::value);
  }

  /* Ambiguous
  {
    boost::unordered_multimap m({std::pair<int const, int>(1,2)}, 0,
  std::hash<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_multimap<int, int,
  std::hash<int>>>::value);
  }

  {
    boost::unordered_multimap m({std::pair<int const, int>(1,2)}, 0,
  std::hash<int>(), std::equal_to<int>());
    static_assert(std::is_same<decltype(m),boost::unordered_multimap<int, int,
  std::hash<int>, std::equal_to<int>>>::value);
  }
  */

  {
    boost::unordered_multimap m(
      {std::pair<int const, int>(1, 2)}, 0, f, f, pair_allocator);
    static_assert(std::is_same<decltype(m),
      boost::unordered_multimap<int, int, hash_equals, hash_equals,
        test_allocator<std::pair<const int, int> > > >::value);
  }

  /* Ambiguous
  {
    boost::unordered_multimap m(x.begin(), x.end(), 0u, pair_allocator);
    static_assert(std::is_same<decltype(m), boost::unordered_multimap<int, int,
  boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  /* No constructor:
  {
    boost::unordered_multimap m(x.begin(), x.end(), pair_allocator);
    static_assert(std::is_same<decltype(m), boost::unordered_multimap<int, int,
  boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  /* Ambiguous
  {
    boost::unordered_multimap m(x.begin(), x.end(), 0u, f, pair_allocator);
    static_assert(std::is_same<decltype(m), boost::unordered_multimap<int, int,
  hash_equals, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }

  {
    boost::unordered_multimap m({std::pair<int const, int>(1,2)}, 0,
  pair_allocator);
    static_assert(std::is_same<decltype(m),boost::unordered_multimap<int, int,
  boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
  int>>>>::value);
  }
  */

  {
    boost::unordered_multimap m(
      {std::pair<int const, int>(1, 2)}, pair_allocator);
    static_assert(std::is_same<decltype(m),
      boost::unordered_multimap<int, int, boost::hash<int>, std::equal_to<int>,
        test_allocator<std::pair<const int, int> > > >::value);
  }

/* Ambiguous
{
  boost::unordered_multimap m({std::pair<int const, int>(1,2)}, 0, f,
pair_allocator);
  static_assert(std::is_same<decltype(m),boost::unordered_multimap<int, int,
boost::hash<int>, std::equal_to<int>, test_allocator<std::pair<const int,
int>>>>::value);
}
*/

#endif
}
