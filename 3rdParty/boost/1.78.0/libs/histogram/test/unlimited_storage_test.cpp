// Copyright 2015-2017 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/detail/detect.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <boost/histogram/unsafe_access.hpp>
#include <boost/mp11.hpp>
#include <iosfwd>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>
#include "std_ostream.hpp"
#include "throw_exception.hpp"
#include "utility_allocator.hpp"

namespace boost {
namespace histogram {
namespace detail {
template <class Allocator>
std::ostream& operator<<(std::ostream& os, const large_int<Allocator>& x) {
  os << "large_int";
  os << x.data;
  return os;
}
} // namespace detail
} // namespace histogram
} // namespace boost

using namespace boost::histogram;

using unlimited_storage_type = unlimited_storage<>;
template <typename T>
using vector_storage = storage_adaptor<std::vector<T>>;
using large_int = unlimited_storage_type::large_int;

template <typename T = std::uint8_t>
unlimited_storage_type prepare(std::size_t n, T x = T{}) {
  std::unique_ptr<T[]> v(new T[n]);
  std::fill(v.get(), v.get() + n, static_cast<T>(0));
  v.get()[0] = x;
  return unlimited_storage_type(n, v.get());
}

template <class T>
auto limits_max() {
  return (std::numeric_limits<T>::max)();
}

template <>
inline auto limits_max<large_int>() {
  return large_int(limits_max<uint64_t>());
}

template <typename T>
void copy() {
  const auto b = prepare<T>(1);
  auto a(b);
  BOOST_TEST(a == b);
  ++a[0];
  BOOST_TEST(!(a == b));
  a = b;
  BOOST_TEST(a == b);
  ++a[0];
  BOOST_TEST(!(a == b));
  a = prepare<T>(2);
  BOOST_TEST(!(a == b));
  a = b;
  BOOST_TEST(a == b);
}

template <typename T>
void equal_1() {
  auto a = prepare(1);
  auto b = prepare(1, T(0));
  BOOST_TEST_EQ(a[0], 0.0);
  BOOST_TEST(a == b);
  ++b[0];
  BOOST_TEST(!(a == b));
}

template <typename T, typename U>
void equal_2() {
  auto a = prepare<T>(1);
  vector_storage<U> b;
  b.reset(1);
  BOOST_TEST(a == b);
  ++b[0];
  BOOST_TEST(!(a == b));
}

template <typename T>
void increase_and_grow() {
  auto tmax = limits_max<T>();
  auto s = prepare(2, tmax);
  auto n = s;
  auto n2 = s;

  ++n[0];

  auto x = prepare(2);
  ++x[0];
  n2[0] += x[0];

  auto v = static_cast<double>(tmax);
  ++v;
  BOOST_TEST_EQ(n[0], v);
  BOOST_TEST_EQ(n2[0], v);
  BOOST_TEST_EQ(n[1], 0.0);
  BOOST_TEST_EQ(n2[1], 0.0);
}

template <typename T>
void convert_foreign_storage() {

  {
    vector_storage<T> s;
    s.reset(1);
    ++s[0];
    BOOST_TEST_EQ(s[0], 1);

    // test converting copy ctor
    unlimited_storage_type u(s);
    using buffer_t = std::decay_t<decltype(unsafe_access::unlimited_storage_buffer(u))>;
    BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(u).type,
                  buffer_t::template type_index<T>());
    BOOST_TEST(u == s);
    BOOST_TEST_EQ(u.size(), 1u);
    BOOST_TEST_EQ(u[0], 1.0);
    ++s[0];
    BOOST_TEST_NOT(u == s);
  }

  vector_storage<uint8_t> s;
  s.reset(1);
  ++s[0];

  // test assign and equal
  auto a = prepare<T>(1);
  a = s;
  BOOST_TEST_EQ(a[0], 1.0);
  BOOST_TEST(a == s);
  ++a[0];
  BOOST_TEST_NOT(a == s);

  // test radd
  auto c = prepare<T>(1);
  c[0] += s[0];
  BOOST_TEST_EQ(c[0], 1);
  BOOST_TEST(c == s);
  c[0] += s[0];
  BOOST_TEST_EQ(c[0], 2);
  BOOST_TEST_NOT(c == s);

  // test assign from float
  vector_storage<float> t;
  t.reset(1);
  t[0] = 1.5;
  auto d = prepare<T>(1);
  d = t;
  BOOST_TEST(d == t);
  BOOST_TEST(d[0] == 1.5);

  // test "copy" ctor from float
  unlimited_storage_type f(t);
  BOOST_TEST_EQ(f[0], 1.5);
  BOOST_TEST(f == t);

  // test radd from float
  auto g = prepare<T>(1);
  g[0] += t[0];
  BOOST_TEST_EQ(g[0], 1.5);
  BOOST_TEST(g == t);

  vector_storage<int8_t> u;
  u.reset(1);
  u[0] = -10;
  auto h = prepare<T>(1);
  BOOST_TEST_NOT(h == u);
  h = u;
  BOOST_TEST(h == u);
  BOOST_TEST_EQ(h[0], -10);
  h[0] -= u[0];
  BOOST_TEST_EQ(h[0], 0);
}

struct adder {
  template <class LHS, class RHS>
  void operator()(boost::mp11::mp_list<LHS, RHS>) {
    using buffer_type =
        std::remove_reference_t<decltype(unsafe_access::unlimited_storage_buffer(
            std::declval<unlimited_storage_type&>()))>;
    constexpr auto iLHS = buffer_type::template type_index<LHS>();
    constexpr auto iRHS = buffer_type::template type_index<RHS>();
    {
      auto a = prepare<LHS>(1);
      BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(a).type, iLHS);
      a[0] += static_cast<RHS>(2);
      // LHS is never downgraded, only upgraded to RHS.
      // If RHS is normal integer, LHS doesn't change.
      BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(a).type,
                    iRHS < 4 ? iLHS : (std::max)(iLHS, iRHS));
      BOOST_TEST_EQ(a[0], 2);
    }
    {
      auto a = prepare<LHS>(1);
      a[0] += 2;
      BOOST_TEST_EQ(a[0], 2);
      // subtracting converts to double
      a[0] -= 2;
      BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(a).type, 5);
      BOOST_TEST_EQ(a[0], 0);
    }
    {
      auto a = prepare<LHS>(1);
      auto b = prepare<RHS>(1, static_cast<RHS>(2u));
      // LHS is never downgraded, only upgraded to RHS.
      // If RHS is normal integer, LHS doesn't change.
      a[0] += b[0];
      BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(a).type,
                    iRHS < 4 ? iLHS : (std::max)(iLHS, iRHS));
      BOOST_TEST_EQ(a[0], 2);
      a[0] -= b[0];
      BOOST_TEST_EQ(a[0], 0);
      a[0] -= b[0];
      BOOST_TEST_EQ(a[0], -2);
    }
    {
      auto a = prepare<LHS>(1);
      auto b = limits_max<RHS>();
      // LHS is never downgraded, only upgraded to RHS.
      // If RHS is normal integer, LHS doesn't change.
      a[0] += b;
      // BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(a).type,
      //               iRHS < 4 ? iLHS : std::max(iLHS, iRHS));
      BOOST_TEST_EQ(a[0], limits_max<RHS>());
      a[0] += prepare<RHS>(1, b)[0];
      // BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(a).type,
      //               iRHS < 4 ? iLHS + 1 : std::max(iLHS, iRHS));
      BOOST_TEST_EQ(a[0], 2 * double(limits_max<RHS>()));
    }
  }
};

int main() {
  // empty state
  {
    unlimited_storage_type a;
    BOOST_TEST_EQ(a.size(), 0);
  }

  // copy
  {
    copy<uint8_t>();
    copy<uint16_t>();
    copy<uint32_t>();
    copy<uint64_t>();
    copy<large_int>();
    copy<double>();
  }

  // equal_operator
  {
    equal_1<uint8_t>();
    equal_1<uint16_t>();
    equal_1<uint32_t>();
    equal_1<uint64_t>();
    equal_1<large_int>();
    equal_1<double>();

    equal_2<uint8_t, unsigned>();
    equal_2<uint16_t, unsigned>();
    equal_2<uint32_t, unsigned>();
    equal_2<uint64_t, unsigned>();
    equal_2<large_int, unsigned>();
    equal_2<double, unsigned>();

    equal_2<large_int, double>();

    auto a = prepare<double>(1);
    auto b = prepare<large_int>(1);
    BOOST_TEST(a == b);
    ++a[0];
    BOOST_TEST_NOT(a == b);
  }

  // increase_and_grow
  {
    increase_and_grow<uint8_t>();
    increase_and_grow<uint16_t>();
    increase_and_grow<uint32_t>();
    increase_and_grow<uint64_t>();

    // only increase for large_int
    auto a = prepare<large_int>(2, static_cast<large_int>(1));
    BOOST_TEST_EQ(a[0], 1);
    BOOST_TEST_EQ(a[1], 0);
    ++a[0];
    BOOST_TEST_EQ(a[0], 2);
    BOOST_TEST_EQ(a[1], 0);
  }

  // add
  {
    using namespace boost::mp11;
    using L = mp_list<uint8_t, uint16_t, uint64_t, large_int, double>;
    mp_for_each<mp_product<mp_list, L, L>>(adder());
  }

  // add_and_grow
  {
    auto a = prepare(1);
    a[0] += a[0];
    BOOST_TEST_EQ(a[0], 0);
    ++a[0];
    double x = 1;
    auto b = prepare(1);
    ++b[0];
    BOOST_TEST_EQ(b[0], x);
    for (unsigned i = 0; i < 80; ++i) {
      x += x;
      a[0] += a[0];
      b[0] += b[0];
      BOOST_TEST_EQ(a[0], x);
      BOOST_TEST_EQ(b[0], x);
      auto c = prepare(1);
      c[0] += a[0];
      BOOST_TEST_EQ(c[0], x);
      c[0] += 0;
      BOOST_TEST_EQ(c[0], x);
      auto d = prepare(1);
      d[0] += x;
      BOOST_TEST_EQ(d[0], x);
    }
  }

  // multiply
  {
    auto a = prepare(2);
    ++a[0];
    a *= 3;
    BOOST_TEST_EQ(a[0], 3);
    BOOST_TEST_EQ(a[1], 0);
    a[1] += 2;
    a *= 3;
    BOOST_TEST_EQ(a[0], 9);
    BOOST_TEST_EQ(a[1], 6);
  }

  // convert_foreign_storage
  {
    convert_foreign_storage<uint8_t>();
    convert_foreign_storage<uint16_t>();
    convert_foreign_storage<uint32_t>();
    convert_foreign_storage<uint64_t>();
    convert_foreign_storage<large_int>();
    convert_foreign_storage<double>();
  }

  // reference
  {
    auto a = prepare(1);
    auto b = prepare<uint32_t>(1);
    BOOST_TEST_EQ(a[0], b[0]);
    BOOST_TEST_GE(a[0], b[0]);
    BOOST_TEST_LE(a[0], b[0]);
    a[0] = 1;
    BOOST_TEST_NE(a[0], b[0]);
    BOOST_TEST_LT(b[0], a[0]);
    BOOST_TEST_GT(a[0], b[0]);
    BOOST_TEST_EQ(a[0], 1);
    BOOST_TEST_GE(a[0], 1);
    BOOST_TEST_LE(a[0], 1);
    BOOST_TEST_NE(a[0], 2);
    BOOST_TEST_GT(2, a[0]);
    BOOST_TEST_LT(0, a[0]);
    BOOST_TEST_GE(1, a[0]);
    BOOST_TEST_GE(2, a[0]);
    BOOST_TEST_LE(0, a[0]);
    BOOST_TEST_LE(1, a[0]);
    BOOST_TEST_EQ(1, a[0]);
    BOOST_TEST_NE(2, a[0]);

    ++b[0];
    BOOST_TEST_EQ(a[0], b[0]);
    b[0] += 2;
    a[0] = b[0];
    BOOST_TEST_EQ(a[0], 3);
    a[0] -= 10;
    BOOST_TEST_EQ(a[0], -7);
    auto c = prepare(2);
    c[0] = c[1] = 1;
    BOOST_TEST_EQ(c[0], 1);
    BOOST_TEST_EQ(c[1], 1);

    auto d = prepare(2);
    d[1] = unlimited_storage_type::large_int{2};
    BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(d).type, 4);
    d[0] = -2;
    BOOST_TEST_EQ(unsafe_access::unlimited_storage_buffer(d).type, 5);
    BOOST_TEST_EQ(d[0], -2);
    BOOST_TEST_EQ(d[1], 2);

    BOOST_TEST_TRAIT_TRUE((detail::has_operator_preincrement<decltype(d[0])>));
  }

  // iterators
  {
    using iterator = typename unlimited_storage_type::iterator;
    using value_type = typename std::iterator_traits<iterator>::value_type;
    using reference = typename std::iterator_traits<iterator>::reference;

    BOOST_TEST_TRAIT_SAME(value_type, double);
    BOOST_TEST_TRAIT_FALSE((std::is_same<reference, double&>));

    auto a = prepare(2);
    for (auto&& x : a) BOOST_TEST_EQ(x, 0);

    std::vector<double> b(2, 1);
    std::copy(b.begin(), b.end(), a.begin());

    const auto& aconst = a;
    BOOST_TEST(std::equal(aconst.begin(), aconst.end(), b.begin(), b.end()));

    unlimited_storage_type::iterator it1 = a.begin();
    BOOST_TEST_EQ(*it1, 1);
    *it1 = 3;
    BOOST_TEST_EQ(*it1, 3);
    unlimited_storage_type::const_iterator it2 = a.begin();
    BOOST_TEST_EQ(*it2, 3);
    unlimited_storage_type::const_iterator it3 = aconst.begin();
    BOOST_TEST_EQ(*it3, 3);

    std::copy(b.begin(), b.end(), a.begin());
    std::partial_sum(a.begin(), a.end(), a.begin());
    BOOST_TEST_EQ(a[0], 1);
    BOOST_TEST_EQ(a[1], 2);
  }

  // memory exhaustion
  {
    using S = unlimited_storage<tracing_allocator<char>>;
    using alloc_t = typename S::allocator_type;
    {
      // check that large_int allocates in ctor
      tracing_allocator_db db;
      typename S::large_int li{1, alloc_t{db}};
      BOOST_TEST_GT(db.first, 0);
    }

    tracing_allocator_db db;
    // db.tracing = true; // uncomment this to monitor allocator activity
    S s(alloc_t{db});
    s.reset(10); // should work
    BOOST_TEST_EQ(db.at<uint8_t>().first, 10);

#ifndef BOOST_NO_EXCEPTIONS
    db.failure_countdown = 0;
    BOOST_TEST_THROWS(s.reset(5), std::bad_alloc);
    // storage must be still in valid state
    BOOST_TEST_EQ(s.size(), 0);
    auto& buffer = unsafe_access::unlimited_storage_buffer(s);
    BOOST_TEST_EQ(buffer.ptr, nullptr);
    BOOST_TEST_EQ(buffer.type, 0);
    // all allocated memory should have returned
    BOOST_TEST_EQ(db.first, 0);

    // test failure in buffer.make<large_int>(n, iter), AT::construct
    s.reset(3);
    s[1] = (std::numeric_limits<std::uint64_t>::max)();
    db.failure_countdown = 2;
    const auto old_ptr = buffer.ptr;
    BOOST_TEST_THROWS(++s[1], std::bad_alloc);

    // storage remains in previous state
    BOOST_TEST_EQ(buffer.size, 3);
    BOOST_TEST_EQ(buffer.ptr, old_ptr);
    BOOST_TEST_EQ(buffer.type, 3);

    // test buffer.make<large_int>(n), AT::construct, called by serialization code
    db.failure_countdown = 1;
    BOOST_TEST_THROWS(buffer.make<typename S::large_int>(2), std::bad_alloc);

    // storage still in valid state
    BOOST_TEST_EQ(s.size(), 0);
    BOOST_TEST_EQ(buffer.ptr, nullptr);
    BOOST_TEST_EQ(buffer.type, 0);
    // all memory returned
    BOOST_TEST_EQ(db.first, 0);
#endif
  }

  return boost::report_errors();
}
