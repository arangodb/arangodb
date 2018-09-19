
// Copyright 2005-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4610) // class can never be instantiated
#pragma warning(disable : 4510) // default constructor could not be generated
#endif

#include <boost/concept_check.hpp>

#if defined(BOOST_MSVC)
#pragma warning(pop)
#endif

#include "../helpers/check_return_type.hpp"
#include <boost/core/pointer_traits.hpp>
#include <boost/limits.hpp>
#include <boost/predef.h>
#include <boost/static_assert.hpp>
#include <boost/type_traits/cv_traits.hpp>
#include <boost/type_traits/is_const.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/utility/swap.hpp>

typedef long double comparison_type;

template <class T> void sink(T const&) {}
template <class T> T rvalue(T const& v) { return v; }
template <class T> T rvalue_default() { return T(); }

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
template <class T> T implicit_construct() { return {}; }
#else
template <class T> int implicit_construct()
{
  T x;
  sink(x);
  return 0;
}
#endif

#if !defined(BOOST_NO_CXX11_NOEXCEPT)
#define TEST_NOEXCEPT_EXPR(x) BOOST_STATIC_ASSERT((BOOST_NOEXCEPT_EXPR(x)));
#else
#define TEST_NOEXCEPT_EXPR(x)
#endif

template <class X, class T> void container_test(X& r, T const&)
{
  typedef typename X::iterator iterator;
  typedef typename X::const_iterator const_iterator;
  typedef typename X::difference_type difference_type;
  typedef typename X::size_type size_type;

  typedef
    typename std::iterator_traits<iterator>::value_type iterator_value_type;
  typedef typename std::iterator_traits<const_iterator>::value_type
    const_iterator_value_type;
  typedef typename std::iterator_traits<iterator>::difference_type
    iterator_difference_type;
  typedef typename std::iterator_traits<const_iterator>::difference_type
    const_iterator_difference_type;

  typedef typename X::value_type value_type;
  typedef typename X::reference reference;
  typedef typename X::const_reference const_reference;

  typedef typename X::node_type node_type;

  typedef typename X::allocator_type allocator_type;

  // value_type

  BOOST_STATIC_ASSERT((boost::is_same<T, value_type>::value));
  boost::function_requires<boost::CopyConstructibleConcept<X> >();

  // reference_type / const_reference_type

  BOOST_STATIC_ASSERT((boost::is_same<T&, reference>::value));
  BOOST_STATIC_ASSERT((boost::is_same<T const&, const_reference>::value));

  // iterator

  boost::function_requires<boost::InputIteratorConcept<iterator> >();
  BOOST_STATIC_ASSERT((boost::is_same<T, iterator_value_type>::value));
  BOOST_STATIC_ASSERT((boost::is_convertible<iterator, const_iterator>::value));

  // const_iterator

  boost::function_requires<boost::InputIteratorConcept<const_iterator> >();
  BOOST_STATIC_ASSERT((boost::is_same<T, const_iterator_value_type>::value));

  // node_type

  BOOST_STATIC_ASSERT((
    boost::is_same<allocator_type, typename node_type::allocator_type>::value));

  // difference_type

  BOOST_STATIC_ASSERT(std::numeric_limits<difference_type>::is_signed);
  BOOST_STATIC_ASSERT(std::numeric_limits<difference_type>::is_integer);
  BOOST_STATIC_ASSERT(
    (boost::is_same<difference_type, iterator_difference_type>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<difference_type, const_iterator_difference_type>::value));

  // size_type

  BOOST_STATIC_ASSERT(!std::numeric_limits<size_type>::is_signed);
  BOOST_STATIC_ASSERT(std::numeric_limits<size_type>::is_integer);

  // size_type can represent any non-negative value type of difference_type
  // I'm not sure about either of these tests...
  size_type max_diff =
    static_cast<size_type>((std::numeric_limits<difference_type>::max)());
  difference_type converted_diff(static_cast<difference_type>(max_diff));
  BOOST_TEST((std::numeric_limits<difference_type>::max)() == converted_diff);

  BOOST_TEST(
    static_cast<comparison_type>((std::numeric_limits<size_type>::max)()) >
    static_cast<comparison_type>(
      (std::numeric_limits<difference_type>::max)()));

// Constructors

// I don't test the runtime post-conditions here.

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
  // It isn't specified in the container requirements that the no argument
  // constructor is implicit, but it is defined that way in the concrete
  // container specification.
  X u_implicit = {};
  sink(u_implicit);
#endif

  X u;
  BOOST_TEST(u.size() == 0);
  BOOST_TEST(X().size() == 0);

  X a, b;
  X a_const;

  sink(X(a));
  X u2(a);
  X u3 = a;
  X u4(rvalue(a_const));
  X u5 = rvalue(a_const);

  a.swap(b);
  boost::swap(a, b);
  test::check_return_type<X>::equals_ref(r = a);

  // Allocator

  test::check_return_type<allocator_type>::equals(a_const.get_allocator());

  allocator_type m = a.get_allocator();
  sink(X(m));
  X c(m);
  sink(X(a_const, m));
  X c2(a_const, m);
  sink(X(rvalue(a_const), m));
  X c3(rvalue(a_const), m);

  // node_type

  implicit_construct<node_type const>();
#if !BOOST_COMP_GNUC || BOOST_COMP_GNUC >= BOOST_VERSION_NUMBER(4, 8, 0)
  TEST_NOEXCEPT_EXPR(node_type());
#endif

  node_type n1;
  node_type n2(rvalue_default<node_type>());
#if !BOOST_COMP_GNUC || BOOST_COMP_GNUC >= BOOST_VERSION_NUMBER(4, 8, 0)
  TEST_NOEXCEPT_EXPR(node_type(boost::move(n1)));
#endif
  node_type n3;
  n3 = boost::move(n2);
  n1.swap(n3);
  swap(n1, n3);
  // TODO: noexcept for swap?
  // value, key, mapped tests in map and set specific testing.

  node_type const n_const;
  BOOST_TEST(n_const ? 0 : 1);
  TEST_NOEXCEPT_EXPR(n_const ? 0 : 1);
  test::check_return_type<bool>::equals(!n_const);
  test::check_return_type<bool>::equals(n_const.empty());
  TEST_NOEXCEPT_EXPR(!n_const);
  TEST_NOEXCEPT_EXPR(n_const.empty());

  // Avoid unused variable warnings:

  sink(u);
  sink(u2);
  sink(u3);
  sink(u4);
  sink(u5);
  sink(c);
  sink(c2);
  sink(c3);
}

template <class X> void unordered_destructible_test(X&)
{
  typedef typename X::iterator iterator;
  typedef typename X::const_iterator const_iterator;
  typedef typename X::size_type size_type;

  X x1;

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
  X x2(rvalue_default<X>());
  X x3 = rvalue_default<X>();
// This can only be done if propagate_on_container_move_assignment::value
// is true.
// x2 = rvalue_default<X>();
#endif

  X* ptr = new X();
  X& a1 = *ptr;
  (&a1)->~X();
  ::operator delete((void*)(&a1));

  X a, b;
  X const a_const;
  test::check_return_type<iterator>::equals(a.begin());
  test::check_return_type<const_iterator>::equals(a_const.begin());
  test::check_return_type<const_iterator>::equals(a.cbegin());
  test::check_return_type<const_iterator>::equals(a_const.cbegin());
  test::check_return_type<iterator>::equals(a.end());
  test::check_return_type<const_iterator>::equals(a_const.end());
  test::check_return_type<const_iterator>::equals(a.cend());
  test::check_return_type<const_iterator>::equals(a_const.cend());

  a.swap(b);
  boost::swap(a, b);

  test::check_return_type<size_type>::equals(a.size());
  test::check_return_type<size_type>::equals(a.max_size());
  test::check_return_type<bool>::convertible(a.empty());

  // Allocator

  typedef typename X::allocator_type allocator_type;
  test::check_return_type<allocator_type>::equals(a_const.get_allocator());
}

template <class X, class Key> void unordered_set_test(X& r, Key const&)
{
  typedef typename X::value_type value_type;
  typedef typename X::key_type key_type;

  BOOST_STATIC_ASSERT((boost::is_same<value_type, key_type>::value));

  // iterator pointer / const_pointer_type

  typedef typename X::iterator iterator;
  typedef typename X::const_iterator const_iterator;
  typedef typename X::local_iterator local_iterator;
  typedef typename X::const_local_iterator const_local_iterator;
  typedef typename std::iterator_traits<iterator>::pointer iterator_pointer;
  typedef typename std::iterator_traits<const_iterator>::pointer
    const_iterator_pointer;
  typedef typename std::iterator_traits<local_iterator>::pointer
    local_iterator_pointer;
  typedef typename std::iterator_traits<const_local_iterator>::pointer
    const_local_iterator_pointer;

  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type const*, iterator_pointer>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type const*, const_iterator_pointer>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type const*, local_iterator_pointer>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type const*, const_local_iterator_pointer>::value));

  // pointer_traits<iterator>

  BOOST_STATIC_ASSERT((boost::is_same<iterator,
    typename boost::pointer_traits<iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type const,
    typename boost::pointer_traits<iterator>::element_type>::value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<iterator>::difference_type>::value));

  // pointer_traits<const_iterator>

  BOOST_STATIC_ASSERT((boost::is_same<const_iterator,
    typename boost::pointer_traits<const_iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type const,
    typename boost::pointer_traits<const_iterator>::element_type>::value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<const_iterator>::difference_type>::value));

  // pointer_traits<local_iterator>

  BOOST_STATIC_ASSERT((boost::is_same<local_iterator,
    typename boost::pointer_traits<local_iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type const,
    typename boost::pointer_traits<local_iterator>::element_type>::value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<local_iterator>::difference_type>::value));

  // pointer_traits<const_local_iterator>

  BOOST_STATIC_ASSERT((boost::is_same<const_local_iterator,
    typename boost::pointer_traits<const_local_iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type const,
    typename boost::pointer_traits<const_local_iterator>::element_type>::
      value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<const_local_iterator>::difference_type>::
      value));

  typedef typename X::node_type node_type;
  typedef typename node_type::value_type node_value_type;
  BOOST_STATIC_ASSERT((boost::is_same<value_type, node_value_type>::value));

  // Call node_type functions.

  test::minimal::constructor_param v;
  Key k_lvalue(v);
  r.emplace(boost::move(k_lvalue));
  node_type n1 = r.extract(r.begin());
  test::check_return_type<value_type>::equals_ref(n1.value());
}

template <class X, class Key, class T>
void unordered_map_test(X& r, Key const& k, T const& v)
{
  typedef typename X::value_type value_type;
  typedef typename X::key_type key_type;

  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type, std::pair<key_type const, T> >::value));

  // iterator pointer / const_pointer_type

  typedef typename X::iterator iterator;
  typedef typename X::const_iterator const_iterator;
  typedef typename X::local_iterator local_iterator;
  typedef typename X::const_local_iterator const_local_iterator;
  typedef typename std::iterator_traits<iterator>::pointer iterator_pointer;
  typedef typename std::iterator_traits<const_iterator>::pointer
    const_iterator_pointer;
  typedef typename std::iterator_traits<local_iterator>::pointer
    local_iterator_pointer;
  typedef typename std::iterator_traits<const_local_iterator>::pointer
    const_local_iterator_pointer;

  BOOST_STATIC_ASSERT((boost::is_same<value_type*, iterator_pointer>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type const*, const_iterator_pointer>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type*, local_iterator_pointer>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<value_type const*, const_local_iterator_pointer>::value));

  // pointer_traits<iterator>

  BOOST_STATIC_ASSERT((boost::is_same<iterator,
    typename boost::pointer_traits<iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type,
    typename boost::pointer_traits<iterator>::element_type>::value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<iterator>::difference_type>::value));

  // pointer_traits<const_iterator>

  BOOST_STATIC_ASSERT((boost::is_same<const_iterator,
    typename boost::pointer_traits<const_iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type const,
    typename boost::pointer_traits<const_iterator>::element_type>::value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<const_iterator>::difference_type>::value));

  // pointer_traits<local_iterator>

  BOOST_STATIC_ASSERT((boost::is_same<local_iterator,
    typename boost::pointer_traits<local_iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type,
    typename boost::pointer_traits<local_iterator>::element_type>::value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<local_iterator>::difference_type>::value));

  // pointer_traits<const_local_iterator>

  BOOST_STATIC_ASSERT((boost::is_same<const_local_iterator,
    typename boost::pointer_traits<const_local_iterator>::pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<value_type const,
    typename boost::pointer_traits<const_local_iterator>::element_type>::
      value));
  BOOST_STATIC_ASSERT((boost::is_same<std::ptrdiff_t,
    typename boost::pointer_traits<const_local_iterator>::difference_type>::
      value));

  typedef typename X::node_type node_type;
  typedef typename node_type::key_type node_key_type;
  typedef typename node_type::mapped_type node_mapped_type;

  BOOST_STATIC_ASSERT((boost::is_same<Key, node_key_type>::value));
  BOOST_STATIC_ASSERT((boost::is_same<T, node_mapped_type>::value));
  // Superfluous,but just to make sure.
  BOOST_STATIC_ASSERT((!boost::is_const<node_key_type>::value));

  // Calling functions

  r.insert(std::pair<Key const, T>(k, v));
  r.insert(r.begin(), std::pair<Key const, T>(k, v));
  std::pair<Key const, T> const value(k, v);
  r.insert(value);
  r.insert(r.end(), value);

  Key k_lvalue(k);
  T v_lvalue(v);

  // Emplace

  r.emplace(k, v);
  r.emplace(k_lvalue, v_lvalue);
  r.emplace(rvalue(k), rvalue(v));

  r.emplace(boost::unordered::piecewise_construct, boost::make_tuple(k),
    boost::make_tuple(v));

  // Emplace with hint

  r.emplace_hint(r.begin(), k, v);
  r.emplace_hint(r.begin(), k_lvalue, v_lvalue);
  r.emplace_hint(r.begin(), rvalue(k), rvalue(v));

  r.emplace_hint(r.begin(), boost::unordered::piecewise_construct,
    boost::make_tuple(k), boost::make_tuple(v));

  // Extract

  test::check_return_type<node_type>::equals(r.extract(r.begin()));

  r.emplace(k, v);
  test::check_return_type<node_type>::equals(r.extract(k));

  r.emplace(k, v);
  node_type n1 = r.extract(r.begin());
  test::check_return_type<key_type>::equals_ref(n1.key());
  test::check_return_type<T>::equals_ref(n1.mapped());

  node_type n2 = boost::move(n1);
  r.insert(boost::move(n2));
  r.insert(r.extract(r.begin()));
  n2 = r.extract(r.begin());
  r.insert(r.begin(), boost::move(n2));
  r.insert(r.end(), r.extract(r.begin()));

  node_type n = r.extract(r.begin());
  test::check_return_type<node_key_type>::equals_ref(n.key());
  test::check_return_type<node_mapped_type>::equals_ref(n.mapped());
}

template <class X> void equality_test(X& r)
{
  X const a = r, b = r;

  test::check_return_type<bool>::equals(a == b);
  test::check_return_type<bool>::equals(a != b);
  test::check_return_type<bool>::equals(boost::operator==(a, b));
  test::check_return_type<bool>::equals(boost::operator!=(a, b));
}

template <class X, class T> void unordered_unique_test(X& r, T const& t)
{
  typedef typename X::iterator iterator;
  test::check_return_type<std::pair<iterator, bool> >::equals(r.insert(t));
  test::check_return_type<std::pair<iterator, bool> >::equals(r.emplace(t));

  typedef typename X::node_type node_type;
  typedef typename X::insert_return_type insert_return_type;

  // insert_return_type

  // TODO;
  // boost::function_requires<
  //     boost::MoveConstructibleConcept<insert_return_type>
  // >();
  // TODO;
  // boost::function_requires<
  //     boost::MoveAssignableConcept<insert_return_type>
  // >();
  boost::function_requires<
    boost::DefaultConstructibleConcept<insert_return_type> >();
  // TODO:
  // boost::function_requires<
  //     boost::DestructibleConcept<insert_return_type>
  // >();
  insert_return_type insert_return, insert_return2;
  test::check_return_type<bool>::equals(insert_return.inserted);
  test::check_return_type<iterator>::equals(insert_return.position);
  test::check_return_type<node_type>::equals_ref(insert_return.node);
  boost::swap(insert_return, insert_return2);
}

template <class X, class T> void unordered_equivalent_test(X& r, T const& t)
{
  typedef typename X::iterator iterator;
  test::check_return_type<iterator>::equals(r.insert(t));
  test::check_return_type<iterator>::equals(r.emplace(t));
}

template <class X, class Key, class T>
void unordered_map_functions(X&, Key const& k, T const& v)
{
  typedef typename X::mapped_type mapped_type;
  typedef typename X::iterator iterator;

  X a;
  test::check_return_type<mapped_type>::equals_ref(a[k]);
  test::check_return_type<mapped_type>::equals_ref(a[rvalue(k)]);
  test::check_return_type<mapped_type>::equals_ref(a.at(k));
  test::check_return_type<std::pair<iterator, bool> >::equals(
    a.try_emplace(k, v));
  test::check_return_type<std::pair<iterator, bool> >::equals(
    a.try_emplace(rvalue(k), v));
  test::check_return_type<iterator>::equals(a.try_emplace(a.begin(), k, v));
  test::check_return_type<iterator>::equals(
    a.try_emplace(a.begin(), rvalue(k), v));
  test::check_return_type<std::pair<iterator, bool> >::equals(
    a.insert_or_assign(k, v));
  test::check_return_type<std::pair<iterator, bool> >::equals(
    a.insert_or_assign(rvalue(k), v));
  test::check_return_type<iterator>::equals(
    a.insert_or_assign(a.begin(), k, v));
  test::check_return_type<iterator>::equals(
    a.insert_or_assign(a.begin(), rvalue(k), v));

  X const b = a;
  test::check_return_type<mapped_type const>::equals_ref(b.at(k));
}

template <class X, class Key, class Hash, class Pred>
void unordered_test(X& x, Key& k, Hash& hf, Pred& eq)
{
  unordered_destructible_test(x);

  typedef typename X::key_type key_type;
  typedef typename X::hasher hasher;
  typedef typename X::key_equal key_equal;
  typedef typename X::size_type size_type;

  typedef typename X::iterator iterator;
  typedef typename X::const_iterator const_iterator;
  typedef typename X::local_iterator local_iterator;
  typedef typename X::const_local_iterator const_local_iterator;

  typedef typename std::iterator_traits<iterator>::iterator_category
    iterator_category;
  typedef typename std::iterator_traits<iterator>::difference_type
    iterator_difference;
  typedef typename std::iterator_traits<iterator>::pointer iterator_pointer;
  typedef typename std::iterator_traits<iterator>::reference iterator_reference;

  typedef typename std::iterator_traits<local_iterator>::iterator_category
    local_iterator_category;
  typedef typename std::iterator_traits<local_iterator>::difference_type
    local_iterator_difference;
  typedef typename std::iterator_traits<local_iterator>::pointer
    local_iterator_pointer;
  typedef typename std::iterator_traits<local_iterator>::reference
    local_iterator_reference;

  typedef typename std::iterator_traits<const_iterator>::iterator_category
    const_iterator_category;
  typedef typename std::iterator_traits<const_iterator>::difference_type
    const_iterator_difference;
  typedef typename std::iterator_traits<const_iterator>::pointer
    const_iterator_pointer;
  typedef typename std::iterator_traits<const_iterator>::reference
    const_iterator_reference;

  typedef typename std::iterator_traits<const_local_iterator>::iterator_category
    const_local_iterator_category;
  typedef typename std::iterator_traits<const_local_iterator>::difference_type
    const_local_iterator_difference;
  typedef typename std::iterator_traits<const_local_iterator>::pointer
    const_local_iterator_pointer;
  typedef typename std::iterator_traits<const_local_iterator>::reference
    const_local_iterator_reference;
  typedef typename X::allocator_type allocator_type;

  BOOST_STATIC_ASSERT((boost::is_same<Key, key_type>::value));
  // boost::function_requires<boost::CopyConstructibleConcept<key_type> >();
  // boost::function_requires<boost::AssignableConcept<key_type> >();

  BOOST_STATIC_ASSERT((boost::is_same<Hash, hasher>::value));
  test::check_return_type<std::size_t>::equals(hf(k));

  BOOST_STATIC_ASSERT((boost::is_same<Pred, key_equal>::value));
  test::check_return_type<bool>::convertible(eq(k, k));

  boost::function_requires<boost::InputIteratorConcept<local_iterator> >();
  BOOST_STATIC_ASSERT(
    (boost::is_same<local_iterator_category, iterator_category>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<local_iterator_difference, iterator_difference>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<local_iterator_pointer, iterator_pointer>::value));
  BOOST_STATIC_ASSERT(
    (boost::is_same<local_iterator_reference, iterator_reference>::value));

  boost::function_requires<
    boost::InputIteratorConcept<const_local_iterator> >();
  BOOST_STATIC_ASSERT((boost::is_same<const_local_iterator_category,
    const_iterator_category>::value));
  BOOST_STATIC_ASSERT((boost::is_same<const_local_iterator_difference,
    const_iterator_difference>::value));
  BOOST_STATIC_ASSERT((boost::is_same<const_local_iterator_pointer,
    const_iterator_pointer>::value));
  BOOST_STATIC_ASSERT((boost::is_same<const_local_iterator_reference,
    const_iterator_reference>::value));

  X a;
  allocator_type m = a.get_allocator();

  // Constructors

  X(10, hf, eq);
  X a1(10, hf, eq);
  X(10, hf);
  X a2(10, hf);
  X(10);
  X a3(10);
  X();
  X a4;

  X(10, hf, eq, m);
  X a1a(10, hf, eq, m);
  X(10, hf, m);
  X a2a(10, hf, m);
  X(10, m);
  X a3a(10, m);
  (X(m));
  X a4a(m);

  test::check_return_type<size_type>::equals(a.erase(k));

  const_iterator q1 = a.cbegin(), q2 = a.cend();
  test::check_return_type<iterator>::equals(a.erase(q1, q2));

  TEST_NOEXCEPT_EXPR(a.clear());
  a.clear();

  X const b;

  test::check_return_type<hasher>::equals(b.hash_function());
  test::check_return_type<key_equal>::equals(b.key_eq());

  test::check_return_type<iterator>::equals(a.find(k));
  test::check_return_type<const_iterator>::equals(b.find(k));
  test::check_return_type<size_type>::equals(b.count(k));
  test::check_return_type<std::pair<iterator, iterator> >::equals(
    a.equal_range(k));
  test::check_return_type<std::pair<const_iterator, const_iterator> >::equals(
    b.equal_range(k));
  test::check_return_type<size_type>::equals(b.bucket_count());
  test::check_return_type<size_type>::equals(b.max_bucket_count());
  test::check_return_type<size_type>::equals(b.bucket(k));
  test::check_return_type<size_type>::equals(b.bucket_size(0));

  test::check_return_type<local_iterator>::equals(a.begin(0));
  test::check_return_type<const_local_iterator>::equals(b.begin(0));
  test::check_return_type<local_iterator>::equals(a.end(0));
  test::check_return_type<const_local_iterator>::equals(b.end(0));

  test::check_return_type<const_local_iterator>::equals(a.cbegin(0));
  test::check_return_type<const_local_iterator>::equals(b.cbegin(0));
  test::check_return_type<const_local_iterator>::equals(a.cend(0));
  test::check_return_type<const_local_iterator>::equals(b.cend(0));

  test::check_return_type<float>::equals(b.load_factor());
  test::check_return_type<float>::equals(b.max_load_factor());
  a.max_load_factor((float)2.0);
  a.rehash(100);

  a.merge(a2);
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
  a.merge(rvalue_default<X>());
#endif

  // Avoid unused variable warnings:

  sink(a);
  sink(a1);
  sink(a2);
  sink(a3);
  sink(a4);
  sink(a1a);
  sink(a2a);
  sink(a3a);
  sink(a4a);
}

template <class X, class Key, class T, class Hash, class Pred>
void unordered_copyable_test(X& x, Key& k, T& t, Hash& hf, Pred& eq)
{
  unordered_test(x, k, hf, eq);

  typedef typename X::iterator iterator;
  typedef typename X::const_iterator const_iterator;
  typedef typename X::allocator_type allocator_type;

  X a;
  allocator_type m = a.get_allocator();

  typename X::value_type* i = 0;
  typename X::value_type* j = 0;

  // Constructors

  X(i, j, 10, hf, eq);
  X a5(i, j, 10, hf, eq);
  X(i, j, 10, hf);
  X a6(i, j, 10, hf);
  X(i, j, 10);
  X a7(i, j, 10);
  X(i, j);
  X a8(i, j);

  X(i, j, 10, hf, eq, m);
  X a5a(i, j, 10, hf, eq, m);
  X(i, j, 10, hf, m);
  X a6a(i, j, 10, hf, m);
  X(i, j, 10, m);
  X a7a(i, j, 10, m);

// Not specified for some reason (maybe ambiguity with another constructor?)
// X(i, j, m);
// X a8a(i, j, m);
// sink(a8a);

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
  std::size_t min_buckets = 10;
  X({t});
  X({t}, min_buckets);
  X({t}, min_buckets, hf);
  X({t}, min_buckets, hf, eq);
  // X({t}, m);
  X({t}, min_buckets, m);
  X({t}, min_buckets, hf, m);
  X({t}, min_buckets, hf, eq, m);
#endif

  X const b;
  sink(X(b));
  X a9(b);
  a = b;

  sink(X(b, m));
  X a9a(b, m);

  X b1;
  b1.insert(t);
  X a9b(b1);
  sink(a9b);
  X a9c(b1, m);
  sink(a9c);

  const_iterator q = a.cbegin();

  test::check_return_type<iterator>::equals(a.insert(q, t));
  test::check_return_type<iterator>::equals(a.emplace_hint(q, t));

  a.insert(i, j);
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
  std::initializer_list<T> list = {t};
  a.insert(list);
  a.insert({t, t, t});

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1900) &&                                   \
  (!defined(__clang__) || __clang_major__ >= 4 ||                              \
    (__clang_major__ == 3 && __clang_minor__ >= 4))
  a.insert({});
  a.insert({t});
  a.insert({t, t});
#endif
#endif

  X a10;
  a10.insert(t);
  q = a10.cbegin();
  test::check_return_type<iterator>::equals(a10.erase(q));

  // Avoid unused variable warnings:

  sink(a);
  sink(a5);
  sink(a6);
  sink(a7);
  sink(a8);
  sink(a9);
  sink(a5a);
  sink(a6a);
  sink(a7a);
  sink(a9a);

  typedef typename X::node_type node_type;
  typedef typename X::allocator_type allocator_type;
  node_type const n_const = a.extract(a.begin());
  test::check_return_type<allocator_type>::equals(n_const.get_allocator());
}

template <class X, class Key, class T, class Hash, class Pred>
void unordered_movable_test(X& x, Key& k, T& /* t */, Hash& hf, Pred& eq)
{
  unordered_test(x, k, hf, eq);

  typedef typename X::iterator iterator;
  typedef typename X::const_iterator const_iterator;
  typedef typename X::allocator_type allocator_type;

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
  X x1(rvalue_default<X>());
  X x2(boost::move(x1));
  x1 = rvalue_default<X>();
  x2 = boost::move(x1);
#endif

  X a;
  allocator_type m = a.get_allocator();

  test::minimal::constructor_param* i = 0;
  test::minimal::constructor_param* j = 0;

  // Constructors

  X(i, j, 10, hf, eq);
  X a5(i, j, 10, hf, eq);
  X(i, j, 10, hf);
  X a6(i, j, 10, hf);
  X(i, j, 10);
  X a7(i, j, 10);
  X(i, j);
  X a8(i, j);

  X(i, j, 10, hf, eq, m);
  X a5a(i, j, 10, hf, eq, m);
  X(i, j, 10, hf, m);
  X a6a(i, j, 10, hf, m);
  X(i, j, 10, m);
  X a7a(i, j, 10, m);

  // Not specified for some reason (maybe ambiguity with another constructor?)
  // X(i, j, m);
  // X a8a(i, j, m);
  // sink(a8a);

  const_iterator q = a.cbegin();

  test::minimal::constructor_param v;
  a.emplace(v);
  test::check_return_type<iterator>::equals(a.emplace_hint(q, v));

  T v1(v);
  a.emplace(boost::move(v1));
  T v2(v);
  a.insert(boost::move(v2));
  T v3(v);
  test::check_return_type<iterator>::equals(a.emplace_hint(q, boost::move(v3)));
  T v4(v);
  test::check_return_type<iterator>::equals(a.insert(q, boost::move(v4)));

  a.insert(i, j);

  X a10;
  T v5(v);
  a10.insert(boost::move(v5));
  q = a10.cbegin();
  test::check_return_type<iterator>::equals(a10.erase(q));

  // Avoid unused variable warnings:

  sink(a);
  sink(a5);
  sink(a6);
  sink(a7);
  sink(a8);
  sink(a5a);
  sink(a6a);
  sink(a7a);
  sink(a10);
}

template <class X, class T> void unordered_set_member_test(X& x, T& t)
{
  X x1(x);
  x1.insert(t);
  x1.begin()->dummy_member();
  x1.cbegin()->dummy_member();
}

template <class X, class T> void unordered_map_member_test(X& x, T& t)
{
  X x1(x);
  x1.insert(t);
  x1.begin()->first.dummy_member();
  x1.cbegin()->first.dummy_member();
  x1.begin()->second.dummy_member();
  x1.cbegin()->second.dummy_member();
}
