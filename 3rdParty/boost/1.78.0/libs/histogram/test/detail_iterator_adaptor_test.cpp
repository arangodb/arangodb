//  (C) Copyright Thomas Witt 2003.
//  (C) Copyright Hans Dembinski 2019.

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for most recent version including documentation.

#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/iterator_adaptor.hpp>
#include <deque>
#include <set>
#include <type_traits>
#include <vector>
#include "std_ostream.hpp"
#include "utility_iterator.hpp"

using namespace boost::histogram;
using boost::histogram::detail::iterator_adaptor;

typedef std::deque<int> storage;
typedef std::deque<int*> pointer_deque;
typedef std::set<storage::iterator> iterator_set;

template <class T>
struct foo;

void blah(int) {}

struct my_gen {
  typedef int result_type;
  my_gen() : n(0) {}
  int operator()() { return ++n; }
  int n;
};

template <class V>
struct ptr_iterator : iterator_adaptor<ptr_iterator<V>, V*> {
private:
  typedef iterator_adaptor<ptr_iterator<V>, V*> super_t;

public:
  using base_type = typename super_t::base_type;

  ptr_iterator() {}
  ptr_iterator(V* d) : super_t(d) {}

  template <class V2, class = std::enable_if_t<std::is_convertible<V2*, V*>::value>>
  ptr_iterator(const ptr_iterator<V2>& x) : super_t(x.base()) {}

  V& operator*() const { return *(this->base()); }
};

template <class Iter>
struct constant_iterator
    : iterator_adaptor<constant_iterator<Iter>, Iter,
                       typename std::iterator_traits<Iter>::value_type const&> {
  typedef iterator_adaptor<constant_iterator<Iter>, Iter,
                           typename std::iterator_traits<Iter>::value_type const&>
      base_t;

  constant_iterator() {}
  constant_iterator(Iter it) : base_t(it) {}

  typename std::iterator_traits<Iter>::value_type const& operator*() const {
    this->base().operator*();
  }
};

struct mutable_it : iterator_adaptor<mutable_it, int*> {
  typedef iterator_adaptor<mutable_it, int*> super_t;

  mutable_it();
  explicit mutable_it(int* p) : super_t(p) {}

  bool equal(mutable_it const& rhs) const { return this->base() == rhs.base(); }
};

struct constant_it : iterator_adaptor<constant_it, int const*> {
  typedef iterator_adaptor<constant_it, int const*> super_t;

  constant_it();
  explicit constant_it(int* p) : super_t(p) {}
  constant_it(mutable_it const& x) : super_t(x.base()) {}

  bool equal(constant_it const& rhs) const { return this->base() == rhs.base(); }
};

template <class T>
class static_object {
public:
  static T& get() {
    static char d[sizeof(T)];
    return *reinterpret_cast<T*>(d);
  }
};

int main() {
  dummyT array[] = {dummyT(0), dummyT(1), dummyT(2), dummyT(3), dummyT(4), dummyT(5)};
  const int N = sizeof(array) / sizeof(dummyT);

  // Test the iterator_adaptor
  {
    ptr_iterator<dummyT> i(array);
    using reference = typename std::iterator_traits<ptr_iterator<dummyT>>::reference;
    using pointer = typename std::iterator_traits<ptr_iterator<dummyT>>::pointer;
    BOOST_TEST_TRAIT_SAME(reference, dummyT&);
    BOOST_TEST_TRAIT_SAME(pointer, dummyT*);

    random_access_iterator_test(i, N, array);

    ptr_iterator<const dummyT> j(array);
    random_access_iterator_test(j, N, array);
    const_nonconst_iterator_test(i, ++j);
  }

  // Test the iterator_traits
  {
    // Test computation of defaults
    typedef ptr_iterator<int> Iter1;
    // don't use std::iterator_traits here to avoid VC++ problems
    BOOST_TEST_TRAIT_SAME(Iter1::value_type, int);
    BOOST_TEST_TRAIT_SAME(Iter1::reference, int&);
    BOOST_TEST_TRAIT_SAME(Iter1::pointer, int*);
    BOOST_TEST_TRAIT_SAME(Iter1::difference_type, std::ptrdiff_t);
  }

  {
    // Test computation of default when the Value is const
    typedef ptr_iterator<int const> Iter1;
    BOOST_TEST_TRAIT_SAME(Iter1::value_type, int);
    BOOST_TEST_TRAIT_SAME(Iter1::reference, const int&);
    BOOST_TEST_TRAIT_SAME(Iter1::pointer, int const*);
  }

  {
    // Test constant iterator idiom
    typedef ptr_iterator<int> BaseIter;
    typedef constant_iterator<BaseIter> Iter;

    BOOST_TEST_TRAIT_SAME(Iter::value_type, int);
    BOOST_TEST_TRAIT_SAME(Iter::reference, int const&);
    BOOST_TEST_TRAIT_SAME(Iter::pointer, int const*);
  }

  // Test the iterator_adaptor
  {
    ptr_iterator<dummyT> i(array);
    random_access_iterator_test(i, N, array);

    ptr_iterator<const dummyT> j(array);
    random_access_iterator_test(j, N, array);
    const_nonconst_iterator_test(i, ++j);
  }

  // check that base_type is correct
  {
    // Test constant iterator idiom
    typedef ptr_iterator<int> BaseIter;

    BOOST_TEST_TRAIT_SAME(BaseIter::base_type, int*);
    BOOST_TEST_TRAIT_SAME(constant_iterator<BaseIter>::base_type, BaseIter);
  }

  {
    int data[] = {49, 77};

    mutable_it i(data);
    constant_it j(data + 1);
    BOOST_TEST(i < j);
    BOOST_TEST(j > i);
    BOOST_TEST(i <= j);
    BOOST_TEST(j >= i);
    BOOST_TEST(j - i == 1);
    BOOST_TEST(i - j == -1);

    constant_it k = i;

    BOOST_TEST(!(i < k));
    BOOST_TEST(!(k > i));
    BOOST_TEST(i <= k);
    BOOST_TEST(k >= i);
    BOOST_TEST(k - i == 0);
    BOOST_TEST(i - k == 0);
  }

  return boost::report_errors();
}
