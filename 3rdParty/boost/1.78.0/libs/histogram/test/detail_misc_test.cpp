// Copyright 2015-2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/histogram/accumulators/weighted_sum.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/detail/common_type.hpp>
#include <boost/histogram/detail/counting_streambuf.hpp>
#include <boost/histogram/detail/index_translator.hpp>
#include <boost/histogram/detail/nonmember_container_access.hpp>
#include <boost/histogram/detail/span.hpp>
#include <boost/histogram/detail/sub_array.hpp>
#include <boost/histogram/fwd.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unlimited_storage.hpp>
#include <iostream>
#include <ostream>
#include "std_ostream.hpp"
#include "throw_exception.hpp"

namespace boost {
namespace histogram {
template <std::size_t N>
std::ostream& operator<<(std::ostream& os, const multi_index<N>& mi) {
  os << "(";
  bool first = true;
  for (auto&& x : mi) {
    if (!first)
      os << " ";
    else
      first = false;
    os << x << ", ";
  }
  os << ")";
  return os;
}

template <std::size_t N, std::size_t M>
bool operator==(const multi_index<N>& a, const multi_index<M>& b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end());
}
} // namespace histogram
} // namespace boost

using namespace boost::histogram;
using namespace boost::histogram::literals;
namespace dtl = boost::histogram::detail;

int main() {
  // literals
  {
    BOOST_TEST_TRAIT_SAME(std::integral_constant<unsigned, 0>, decltype(0_c));
    BOOST_TEST_TRAIT_SAME(std::integral_constant<unsigned, 3>, decltype(3_c));
    BOOST_TEST_EQ(decltype(10_c)::value, 10);
    BOOST_TEST_EQ(decltype(213_c)::value, 213);
  }

  // common_storage
  {
    BOOST_TEST_TRAIT_SAME(dtl::common_storage<unlimited_storage<>, unlimited_storage<>>,
                          unlimited_storage<>);
    BOOST_TEST_TRAIT_SAME(
        dtl::common_storage<dense_storage<double>, dense_storage<double>>,
        dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(dtl::common_storage<dense_storage<int>, dense_storage<double>>,
                          dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(dtl::common_storage<dense_storage<double>, dense_storage<int>>,
                          dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(dtl::common_storage<dense_storage<double>, unlimited_storage<>>,
                          dense_storage<double>);
    BOOST_TEST_TRAIT_SAME(dtl::common_storage<dense_storage<int>, unlimited_storage<>>,
                          unlimited_storage<>);
    BOOST_TEST_TRAIT_SAME(dtl::common_storage<dense_storage<double>, weight_storage>,
                          weight_storage);
  }

  // size & data
  {
    char a[4] = {1, 2, 3, 4};
    BOOST_TEST_EQ(dtl::size(a), 4u);
    BOOST_TEST_EQ(dtl::data(a), a);
    auto b = {1, 2};
    BOOST_TEST_EQ(dtl::size(b), 2u);
    BOOST_TEST_EQ(dtl::data(b), b.begin());
    struct C {
      unsigned size() const { return 3; }
      int* data() { return buf; }
      const int* data() const { return buf; }
      int buf[1];
    } c;
    BOOST_TEST_EQ(dtl::size(c), 3u);
    BOOST_TEST_EQ(dtl::data(c), c.buf);
    BOOST_TEST_EQ(dtl::data(static_cast<const C&>(c)), c.buf);
    struct {
      int size() const { return 5; }
    } d;
    BOOST_TEST_EQ(dtl::size(d), 5u);
  }

  // counting_streambuf
  {
    std::streamsize count = 0;
    dtl::counting_streambuf<char> csb(count);
    std::ostream os(&csb);
    os.put('x');
    BOOST_TEST_EQ(count, 1);
    os << 12;
    BOOST_TEST_EQ(count, 3);
    os << "123";
    BOOST_TEST_EQ(count, 6);
  }
  {
    std::streamsize count = 0;
    auto g = dtl::make_count_guard(std::cout, count);
    std::cout.put('x');
    BOOST_TEST_EQ(count, 1);
    std::cout << 12;
    BOOST_TEST_EQ(count, 3);
    std::cout << "123";
    BOOST_TEST_EQ(count, 6);
  }

  // sub_array and span
  {
    dtl::sub_array<int, 2> a(2, 1);
    a[1] = 2;
    auto sp = dtl::span<int>(a);
    BOOST_TEST_EQ(sp.size(), 2);
    BOOST_TEST_EQ(sp.front(), 1);
    BOOST_TEST_EQ(sp.back(), 2);

    const auto& ca = a;
    auto csp = dtl::span<const int>(ca);
    BOOST_TEST_EQ(csp.size(), 2);
    BOOST_TEST_EQ(csp.front(), 1);
    BOOST_TEST_EQ(csp.back(), 2);
  }

  // index_translator
  {
    using I = axis::integer<>;

    {
      auto t = std::vector<I>{I{0, 1}, I{1, 3}};
      auto tr = dtl::make_index_translator(t, t);
      multi_index<static_cast<std::size_t>(-1)> mi{0, 1};
      BOOST_TEST_EQ(tr(mi), mi);
      multi_index<2> mi2{0, 1};
      BOOST_TEST_EQ(tr(mi2), mi);
    }

    {
      auto t = std::make_tuple(I{0, 1});
      auto tr = dtl::make_index_translator(t, t);
      multi_index<static_cast<std::size_t>(-1)> mi{0};
      BOOST_TEST_EQ(tr(mi), mi);
    }

    BOOST_TEST_THROWS(multi_index<1>::create(2), std::invalid_argument);
  }

  return boost::report_errors();
}
