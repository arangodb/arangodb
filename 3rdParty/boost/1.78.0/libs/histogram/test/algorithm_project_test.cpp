// Copyright 2015-2018 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/histogram/algorithm/project.hpp>
#include <boost/histogram/algorithm/sum.hpp>
#include <boost/histogram/axis/integer.hpp>
#include <boost/histogram/axis/ostream.hpp>
#include <boost/histogram/literals.hpp>
#include <boost/histogram/ostream.hpp>
#include <vector>
#include "throw_exception.hpp"
#include "utility_histogram.hpp"

using namespace boost::histogram;
using namespace boost::histogram::literals; // to get _c suffix
using namespace boost::histogram::algorithm;

template <typename Tag>
void run_tests() {
  {
    auto h = make(Tag(), axis::integer<>(0, 2), axis::integer<>(0, 3));
    h(0, 0);
    h(0, 1);
    h(1, 0);
    h(1, 1);
    h(1, 2);
    h(1, 2);

    /*
      matrix layout:

      x ->
    y 1 1
    | 1 1
    v 0 2
    */

    auto hx = project(h, 0_c);
    BOOST_TEST_EQ(hx.rank(), 1);
    BOOST_TEST_EQ(sum(hx), 6);
    BOOST_TEST_EQ(hx.axis(), h.axis(0_c));
    BOOST_TEST_EQ(hx.at(0), 2);
    BOOST_TEST_EQ(hx.at(1), 4);

    auto hy = project(h, 1_c);
    BOOST_TEST_EQ(hy.rank(), 1);
    BOOST_TEST_EQ(sum(hy), 6);
    BOOST_TEST_EQ(hy.axis(), h.axis(1_c));
    BOOST_TEST_EQ(hy.at(0), 2);
    BOOST_TEST_EQ(hy.at(1), 2);
    BOOST_TEST_EQ(hy.at(2), 2);

    auto hyx = project(h, 1_c, 0_c);
    BOOST_TEST_EQ(hyx.rank(), 2);
    BOOST_TEST_EQ(sum(hyx), 6);
    BOOST_TEST_EQ(hyx.axis(0_c), h.axis(1_c));
    BOOST_TEST_EQ(hyx.axis(1_c), h.axis(0_c));
    BOOST_TEST_EQ(hyx.at(0, 0), 1);
    BOOST_TEST_EQ(hyx.at(1, 0), 1);
    BOOST_TEST_EQ(hyx.at(2, 0), 0);
    BOOST_TEST_EQ(hyx.at(0, 1), 1);
    BOOST_TEST_EQ(hyx.at(1, 1), 1);
    BOOST_TEST_EQ(hyx.at(2, 1), 2);
  }

  {
    auto h =
        make(Tag(), axis::integer<>(0, 2), axis::integer<>(0, 3), axis::integer<>(0, 4));
    h(0, 0, 0);
    h(0, 1, 0);
    h(0, 1, 1);
    h(0, 0, 2);
    h(1, 0, 2);

    auto h_0 = project(h, 0_c);
    BOOST_TEST_EQ(h_0.rank(), 1);
    BOOST_TEST_EQ(sum(h_0), 5);
    BOOST_TEST_EQ(h_0.at(0), 4);
    BOOST_TEST_EQ(h_0.at(1), 1);
    BOOST_TEST_EQ(h_0.axis(), axis::integer<>(0, 2));

    auto h_1 = project(h, 1_c);
    BOOST_TEST_EQ(h_1.rank(), 1);
    BOOST_TEST_EQ(sum(h_1), 5);
    BOOST_TEST_EQ(h_1.at(0), 3);
    BOOST_TEST_EQ(h_1.at(1), 2);
    BOOST_TEST_EQ(h_1.axis(), axis::integer<>(0, 3));

    auto h_2 = project(h, 2_c);
    BOOST_TEST_EQ(h_2.rank(), 1);
    BOOST_TEST_EQ(sum(h_2), 5);
    BOOST_TEST_EQ(h_2.at(0), 2);
    BOOST_TEST_EQ(h_2.at(1), 1);
    BOOST_TEST_EQ(h_2.at(2), 2);
    BOOST_TEST_EQ(h_2.axis(), axis::integer<>(0, 4));

    auto h_01 = project(h, 0_c, 1_c);
    BOOST_TEST_EQ(h_01.rank(), 2);
    BOOST_TEST_EQ(sum(h_01), 5);
    BOOST_TEST_EQ(h_01.at(0, 0), 2);
    BOOST_TEST_EQ(h_01.at(0, 1), 2);
    BOOST_TEST_EQ(h_01.at(1, 0), 1);
    BOOST_TEST_EQ(h_01.axis(0_c), axis::integer<>(0, 2));
    BOOST_TEST_EQ(h_01.axis(1_c), axis::integer<>(0, 3));

    auto h_02 = project(h, 0_c, 2_c);
    BOOST_TEST_EQ(h_02.rank(), 2);
    BOOST_TEST_EQ(sum(h_02), 5);
    BOOST_TEST_EQ(h_02.at(0, 0), 2);
    BOOST_TEST_EQ(h_02.at(0, 1), 1);
    BOOST_TEST_EQ(h_02.at(0, 2), 1);
    BOOST_TEST_EQ(h_02.at(1, 2), 1);
    BOOST_TEST_EQ(h_02.axis(0_c), axis::integer<>(0, 2));
    BOOST_TEST_EQ(h_02.axis(1_c), axis::integer<>(0, 4));

    auto h_12 = project(h, 1_c, 2_c);
    BOOST_TEST_EQ(h_12.rank(), 2);
    BOOST_TEST_EQ(sum(h_12), 5);
    BOOST_TEST_EQ(h_12.at(0, 0), 1);
    BOOST_TEST_EQ(h_12.at(1, 0), 1);
    BOOST_TEST_EQ(h_12.at(1, 1), 1);
    BOOST_TEST_EQ(h_12.at(0, 2), 2);
    BOOST_TEST_EQ(h_12.axis(0_c), axis::integer<>(0, 3));
    BOOST_TEST_EQ(h_12.axis(1_c), axis::integer<>(0, 4));

    auto h_210 = project(h, 2_c, 1_c, 0_c);
    BOOST_TEST_EQ(h_210.at(0, 0, 0), 1);
    BOOST_TEST_EQ(h_210.at(0, 1, 0), 1);
    BOOST_TEST_EQ(h_210.at(1, 1, 0), 1);
    BOOST_TEST_EQ(h_210.at(2, 0, 0), 1);
    BOOST_TEST_EQ(h_210.at(2, 0, 1), 1);
  }

  {
    auto h = make(dynamic_tag(), axis::integer<>(0, 2), axis::integer<>(0, 3));
    h(0, 0);
    h(0, 1);
    h(1, 0);
    h(1, 1);
    h(1, 2);
    h(1, 2);

    std::vector<int> x;

    x = {0};
    auto hx = project(h, x);
    BOOST_TEST_EQ(hx.rank(), 1);
    BOOST_TEST_EQ(sum(hx), 6);
    BOOST_TEST_EQ(hx.at(0), 2);
    BOOST_TEST_EQ(hx.at(1), 4);
    BOOST_TEST(hx.axis() == h.axis(0_c));

    x = {1};
    auto hy = project(h, x);
    BOOST_TEST_EQ(hy.rank(), 1);
    BOOST_TEST_EQ(sum(hy), 6);
    BOOST_TEST_EQ(hy.at(0), 2);
    BOOST_TEST_EQ(hy.at(1), 2);
    BOOST_TEST_EQ(hy.at(2), 2);
    BOOST_TEST(hy.axis() == h.axis(1_c));

    x = {1, 0};
    auto hyx = project(h, x);
    BOOST_TEST_EQ(hyx.rank(), 2);
    BOOST_TEST_EQ(hyx.axis(0_c), h.axis(1_c));
    BOOST_TEST_EQ(hyx.axis(1_c), h.axis(0_c));
    BOOST_TEST_EQ(sum(hyx), 6);
    BOOST_TEST_EQ(hyx.at(0, 0), 1);
    BOOST_TEST_EQ(hyx.at(1, 0), 1);
    BOOST_TEST_EQ(hyx.at(0, 1), 1);
    BOOST_TEST_EQ(hyx.at(1, 1), 1);
    BOOST_TEST_EQ(hyx.at(2, 1), 2);

    // indices must be unique
    x = {0, 0};
    BOOST_TEST_THROWS((void)project(h, x), std::invalid_argument);

    // indices must be valid
    x = {2, 1};
    BOOST_TEST_THROWS((void)project(h, x), std::invalid_argument);
  }
}

int main() {
  run_tests<static_tag>();
  run_tests<dynamic_tag>();

  return boost::report_errors();
}
