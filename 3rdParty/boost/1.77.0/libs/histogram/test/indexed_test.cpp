// Copyright 2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/axis/variable.hpp>
#include <boost/histogram/histogram.hpp>
#include <boost/histogram/indexed.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <iterator>
#include <ostream>
#include <type_traits>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;
using namespace boost::histogram::literals;
using namespace boost::mp11;

template <class Tag, class Coverage>
void run_1d_tests(mp_list<Tag, Coverage>) {
  auto h = make(Tag(), axis::integer<>(0, 3));
  h(-1, weight(1));
  h(0, weight(2));
  h(1, weight(3));
  h(2, weight(4));
  h(3, weight(5));

  auto ind = indexed(h, Coverage());
  auto it = ind.begin();
  BOOST_TEST_EQ(it->indices().size(), 1);
  BOOST_TEST_EQ(it->indices()[0], Coverage() == coverage::all ? -1 : 0);

  if (Coverage() == coverage::all) {
    BOOST_TEST_EQ(it->index(0), -1);
    BOOST_TEST_EQ(**it, 1);
    BOOST_TEST_EQ(it->bin(0), h.axis().bin(-1));
    ++it;
  }
  BOOST_TEST_EQ(it->index(0), 0);
  BOOST_TEST_EQ(**it, 2);
  BOOST_TEST_EQ(it->bin(0), h.axis().bin(0));
  ++it;
  BOOST_TEST_EQ(it->index(0), 1);
  BOOST_TEST_EQ(**it, 3);
  BOOST_TEST_EQ(it->bin(0), h.axis().bin(1));
  ++it;
  // check post-increment
  auto prev = it++;
  BOOST_TEST_EQ(prev->index(0), 2);
  BOOST_TEST_EQ(**prev, 4);
  BOOST_TEST_EQ(prev->bin(0), h.axis().bin(2));
  if (Coverage() == coverage::all) {
    BOOST_TEST_EQ(it->index(0), 3);
    BOOST_TEST_EQ(**it, 5);
    BOOST_TEST_EQ(it->bin(0), h.axis().bin(3));
    ++it;
  }
  BOOST_TEST(it == ind.end());

  for (auto&& x : indexed(h, Coverage())) *x = 0;

  for (auto&& x : indexed(static_cast<const decltype(h)&>(h), Coverage()))
    BOOST_TEST_EQ(*x, 0);
}

template <class Tag, class Coverage>
void run_3d_tests(mp_list<Tag, Coverage>) {
  auto h = make_s(Tag(), std::vector<int>(), axis::integer<>(0, 2),
                  axis::integer<int, axis::null_type, axis::option::none_t>(0, 3),
                  axis::integer<int, axis::null_type, axis::option::overflow_t>(0, 4));

  for (int i = -1; i < 3; ++i)
    for (int j = -1; j < 4; ++j)
      for (int k = -1; k < 5; ++k) h(i, j, k, weight(i * 100 + j * 10 + k));

  auto ind = indexed(h, Coverage());
  auto it = ind.begin();
  BOOST_TEST_EQ(it->indices().size(), 3);

  const int d = Coverage() == coverage::all;

  // imitate iteration order of indexed loop
  for (int k = 0; k < 4 + d; ++k)
    for (int j = 0; j < 3; ++j)
      for (int i = -d; i < 2 + d; ++i) {
        BOOST_TEST_EQ(it->index(0), i);
        BOOST_TEST_EQ(it->index(1), j);
        BOOST_TEST_EQ(it->index(2), k);
        BOOST_TEST_EQ(it->bin(0_c), h.axis(0_c).bin(i));
        BOOST_TEST_EQ(it->bin(1_c), h.axis(1_c).bin(j));
        BOOST_TEST_EQ(it->bin(2_c), h.axis(2_c).bin(k));
        BOOST_TEST_EQ(**it, i * 100 + j * 10 + k);
        ++it;
      }
  BOOST_TEST(it == ind.end());
}

template <class Tag, class Coverage>
void run_density_tests(mp_list<Tag, Coverage>) {
  auto ax = axis::variable<>({0.0, 0.1, 0.3, 0.6});
  auto ay = axis::integer<int>(0, 2);
  auto az = ax;
  auto h = make_s(Tag(), std::vector<int>(), ax, ay, az);

  // fill uniformly
  for (auto&& x : h) x = 1;

  for (auto&& x : indexed(h, Coverage())) {
    BOOST_TEST_EQ(x.density(), *x / (x.bin(0).width() * x.bin(2).width()));
  }
}

template <class Tag, class Coverage>
void run_stdlib_tests(mp_list<Tag, Coverage>) {
  auto ax = axis::regular<>(3, 0, 1);
  auto ay = axis::integer<>(0, 2);
  auto h = make_s(Tag(), std::array<int, 20>(), ax, ay);

  struct generator {
    int i = 0;
    int operator()() { return ++i; }
  };

  auto ind = indexed(h, Coverage());
  std::generate(ind.begin(), ind.end(), generator{});

  {
    int i = 0;
    for (auto&& x : ind) BOOST_TEST_EQ(*x, ++i);
  }

  {
    auto it = std::min_element(ind.begin(), ind.end());
    BOOST_TEST(it == ind.begin());
    BOOST_TEST(it != ind.end());
  }

  {
    auto it = std::max_element(ind.begin(), ind.end());
    // get last before end()
    auto it2 = ind.begin();
    auto it3 = it2;
    while (it2 != ind.end()) it3 = it2++;
    BOOST_TEST(it == it3);
    BOOST_TEST(it != ind.begin());
  }
}

template <class Tag>
void run_indexed_with_range_tests(Tag) {
  {
    auto ax = axis::integer<>(0, 4);
    auto ay = axis::integer<>(0, 3);

    auto h = make(Tag(), ax, ay);
    for (auto&& x : indexed(h, coverage::all)) x = x.index(0) + x.index(1);
    BOOST_TEST_EQ(h.at(0, 0), 0);
    BOOST_TEST_EQ(h.at(0, 1), 1);
    BOOST_TEST_EQ(h.at(0, 2), 2);
    BOOST_TEST_EQ(h.at(1, 0), 1);
    BOOST_TEST_EQ(h.at(1, 1), 2);
    BOOST_TEST_EQ(h.at(1, 2), 3);
    BOOST_TEST_EQ(h.at(2, 0), 2);
    BOOST_TEST_EQ(h.at(2, 1), 3);
    BOOST_TEST_EQ(h.at(2, 2), 4);
    BOOST_TEST_EQ(h.at(3, 0), 3);
    BOOST_TEST_EQ(h.at(3, 1), 4);
    BOOST_TEST_EQ(h.at(3, 2), 5);

    /* x->
    y  0 1 2 3
    |  1 2 3 4
    v  2 3 4 5
    */

    int range[2][2] = {{1, 3}, {0, 2}};
    auto ir = indexed(h, range);
    auto sum = std::accumulate(ir.begin(), ir.end(), 0.0);
    BOOST_TEST_EQ(sum, 1 + 2 + 2 + 3);
  }
}

int main() {
  mp_for_each<mp_product<mp_list, mp_list<static_tag, dynamic_tag>,
                         mp_list<std::integral_constant<coverage, coverage::inner>,
                                 std::integral_constant<coverage, coverage::all>>>>(
      [](auto&& x) {
        run_1d_tests(x);
        run_3d_tests(x);
        run_density_tests(x);
        run_stdlib_tests(x);
      });

  run_indexed_with_range_tests(static_tag{});
  run_indexed_with_range_tests(dynamic_tag{});
  return boost::report_errors();
}
