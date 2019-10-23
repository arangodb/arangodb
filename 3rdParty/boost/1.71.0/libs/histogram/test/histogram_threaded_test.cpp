// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/accumulators/thread_safe.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <iostream>
#include <random>
#include <thread>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;

constexpr auto n_fill = 400000;

template <class Tag, class A1, class A2, class X, class Y>
void fill_test(const A1& a1, const A2& a2, const X& x, const Y& y) {
  auto h1 = make_s(Tag{}, dense_storage<int>(), a1, a2);
  for (unsigned i = 0; i != n_fill; ++i) h1(x[i], y[i]);
  auto h2 = make_s(Tag{}, dense_storage<accumulators::thread_safe<int>>(), a1, a2);
  auto run = [&h2, &x, &y](int k) {
    constexpr auto shift = n_fill / 4;
    auto xit = x.cbegin() + k * shift;
    auto yit = y.cbegin() + k * shift;
    for (unsigned i = 0; i < shift; ++i) h2(*xit++, *yit++);
  };

  std::thread t1([&] { run(0); });
  std::thread t2([&] { run(1); });
  std::thread t3([&] { run(2); });
  std::thread t4([&] { run(3); });
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  BOOST_TEST_EQ(algorithm::sum(h1), n_fill);
  BOOST_TEST_EQ(algorithm::sum(h2), n_fill);
  BOOST_TEST_EQ(h1, h2);
}

template <class T>
void tests() {
  std::mt19937 gen(1);
  std::uniform_int_distribution<> id(-5, 5);
  std::vector<int> vi(n_fill), vj(n_fill);
  std::generate(vi.begin(), vi.end(), [&] { return id(gen); });
  std::generate(vj.begin(), vj.end(), [&] { return id(gen); });

  using i = axis::integer<>;
  using ig = axis::integer<int, use_default, axis::option::growth_t>;
  fill_test<T>(i{0, 1}, i{0, 1}, vi, vj);
  fill_test<T>(ig{0, 1}, i{0, 1}, vi, vj);
  fill_test<T>(i{0, 1}, ig{0, 1}, vi, vj);
  fill_test<T>(ig{0, 1}, ig{0, 1}, vi, vj);
}

int main() {
  tests<static_tag>();
  tests<dynamic_tag>();

  return boost::report_errors();
}
